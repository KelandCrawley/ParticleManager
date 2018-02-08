#include "ParticleManager.h"



ParticleManager::ParticleManager()
{
    m_rainTexture = nullptr;
    m_fireTexture = nullptr;
    m_defaultTexture = nullptr;
    m_particleList = nullptr;
    m_vertices = nullptr;
    m_Instances = nullptr;
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;
    m_instanceBuffer = nullptr;

}


ParticleManager::~ParticleManager()
{
}


bool ParticleManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* defaultTextureFilename, const char* rainTextureFilename, const char* fireTextureFilename)
{
    bool result;

    //texture loads
    result = LoadTexture(device, deviceContext, fireTextureFilename, &m_fireTexture);
    if (!result)
    {
       return false;
    }

    result = LoadTexture(device, deviceContext, defaultTextureFilename, &m_defaultTexture);
    if (!result)
    {
        return false;
    }

    result = LoadTexture(device, deviceContext, rainTextureFilename, &m_rainTexture);
    if (!result)
    {
        return false;
    }

    result = InitializeParticleSystem();
    if (!result)
    {
        return false;
    }

    result = InitializeBuffers(device);
    if (!result)
    {
        return false;
    }

    //start the rain particles in motion
    InitiateRainEffects();
    return true;
}


void ParticleManager::Shutdown()
{
    ShutdownBuffers();
    ShutdownParticleSystem();
    ReleaseTextures();

    return;
}


bool ParticleManager::Frame(ID3D11DeviceContext* deviceContext, float frameTime)
{
    bool result;

    // deallocate or repeat particles
    KillParticles();

    //create additional fire particles
    MakeFireEffect(XMFLOAT3(3.0f, 0.0f, 28.0f), frameTime);

    // Update the position of the particles.
    UpdateParticles(frameTime);

    // Update the dynamic vertex buffer with the new position of each particle.
    result = UpdateBuffers(deviceContext);
    if (!result)
    {
        return false;
    }

    return true;
}


void ParticleManager::Render(ID3D11DeviceContext* deviceContext)
{
    RenderBuffers(deviceContext);
    return;
}


ID3D11ShaderResourceView * ParticleManager::GetDefaultTexture()
{
    return m_defaultTexture->GetTexture();
}

ID3D11ShaderResourceView * ParticleManager::GetRainTexture()
{
    return m_rainTexture->GetTexture();
}

ID3D11ShaderResourceView * ParticleManager::GetFireTexture()
{
    return m_fireTexture->GetTexture();
}

int ParticleManager::GetIndexCount()
{
    return m_indexCount;
}

int ParticleManager::GetVertexCount()
{
    return m_vertexCount;
}

int ParticleManager::GetRainInstanceCount()
{
    return m_rainInstanceCount;
}

int ParticleManager::GetFireInstanceCount()
{
    return m_fireInstanceCount;
}


int ParticleManager::GetTotalInstanceCount()
{
    return m_totalInstanceCount;
}

int ParticleManager::GetActiveInstanceCount()
{
    return m_activeParticles;
}

bool ParticleManager::LoadTexture(ID3D11Device * device, ID3D11DeviceContext * deviceContext, const char * filename, TextureClass **texture)
{
    bool result;
    (*texture) = new TextureClass;
    if (!(*texture))
    {
        return false;
    }
    result = (*texture)->Initialize(device, deviceContext, filename);
    if (!result)
    {
        return false;
    }

    return true;
}

void ParticleManager::ReleaseTextures()
{
    if (m_defaultTexture)
    {
        m_defaultTexture->Shutdown();
        delete m_defaultTexture;
        m_defaultTexture = 0;
    }
    if (m_rainTexture)
    {
        m_rainTexture->Shutdown();
        delete m_rainTexture;
        m_rainTexture = 0;
    }
    if (m_fireTexture)
    {
        m_fireTexture->Shutdown();
        delete m_fireTexture;
        m_fireTexture = 0;
    }

   return;
}


bool ParticleManager::InitializeParticleSystem()
{
    // Set the default size of Particles
    m_particleSize = 0.10f;

    // Set the maximum number of particles allowed
    m_maxParticles = 10000;
    m_activeParticles = 0;

    m_rainInstanceCount = 1000;
    m_fireInstanceCount = 0;
    m_fireParticlesPerSecond = 150;
    m_rainSpawnInHeight = 20.0f;
    m_rainSpawnYVelocity = -3.0f;
    m_smokeLifeTime = 3.0f;

    m_rainBoxCoordinates[0] = -10.0f;
    m_rainBoxCoordinates[1] =  20.0f;
    m_rainBoxCoordinates[2] =  15.0f;
    m_rainBoxCoordinates[3] =  50.0f;

    //set the value of gravity
    m_gravityConstant = -3.5f;

    //List heads set to null
    m_headOfFreeList = nullptr;
    m_headOfAllocatedList = nullptr;
    m_headOfRainAllocatedList = nullptr;
    m_headOfFireAllocatedList = nullptr;


    // Create the particle list.
    m_particleList = new Particle[m_maxParticles];
    if (!m_particleList)
    {
        return false;
    }
    m_headOfFreeList = &m_particleList[0]; // pointer to memory adress

    // Each particle points to the next. this will setup the default connections of the free particle list
    for (auto i = 0; i < m_maxParticles - 1; ++i)
    {
        m_particleList[i].next = &m_particleList[i + 1];
    }
    m_particleList[m_maxParticles - 1].next = nullptr; // last node

    return true;
}

void ParticleManager::ShutdownParticleSystem()
{
    if (m_particleList)
    {
        delete[] m_particleList;
        m_particleList = 0;
    }
    return;
}


bool ParticleManager::InitializeBuffers(ID3D11Device* device)
{
    D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc, instanceBufferDesc;
    D3D11_SUBRESOURCE_DATA vertexData, indexData, instanceData;
    HRESULT result;


    // Max vertex is set to 6 times the number of different vertex setups
    m_vertexCount = 18;
    m_indexCount = m_vertexCount;

    m_vertices = new VertexType[m_vertexCount];
    if (!m_vertices)
    {
        return false;
    }

    unsigned long* indices = new unsigned long[m_indexCount];
    if (!indices)
    {
        return false;
    }

    memset(m_vertices, 0, (sizeof(VertexType) * m_vertexCount));

    // Initialize the index array.
    for (auto i = 0; i < 12; i++)
    {
        indices[i] = i;
    }

    //default values should not be used as per instance data will replace it per frame
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float red = 0.5f;
    float green = 0.5f;
    float blue = 0.5f;
    int index = 0;

    // default particles, currently used be rain droplets
    m_particleSize = 0.015f;
        
        // Bottom left.
        m_vertices[index].position = XMFLOAT3(positionX - m_particleSize, (positionY - m_particleSize), positionZ);
        m_vertices[index].texture = XMFLOAT2(0.0f, 1.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Top left.
        m_vertices[index].position = XMFLOAT3(positionX - m_particleSize, (positionY + m_particleSize), positionZ);
        m_vertices[index].texture = XMFLOAT2(0.0f, 0.0f);
        m_vertices[index].color = XMFLOAT4(red,green, blue, 1.0f);
        index++;

        // Bottom right.
        m_vertices[index].position = XMFLOAT3(positionX + m_particleSize, (positionY - m_particleSize), positionZ);
        m_vertices[index].texture = XMFLOAT2(1.0f, 1.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Bottom right.
        m_vertices[index].position = XMFLOAT3(positionX + m_particleSize, (positionY - m_particleSize), positionZ);
        m_vertices[index].texture = XMFLOAT2(1.0f, 1.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Top left.
        m_vertices[index].position = XMFLOAT3(positionX - m_particleSize, (positionY + m_particleSize), positionZ);
        m_vertices[index].texture = XMFLOAT2(0.0f, 0.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Top right.
        m_vertices[index].position = XMFLOAT3(positionX + m_particleSize, (positionY + m_particleSize), positionZ);
        m_vertices[index].texture = XMFLOAT2(1.0f, 0.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;


        //rain, long rectangle shape made of 2 triangles

        m_particleSize = 0.010f;
        // Bottom left.
        m_vertices[index].position = XMFLOAT3(positionX - m_particleSize, (positionY - (m_particleSize * 16)), positionZ);
        m_vertices[index].texture = XMFLOAT2(0.0f, 1.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Top left.
        m_vertices[index].position = XMFLOAT3(positionX - m_particleSize, (positionY + (m_particleSize * 16)), positionZ);
        m_vertices[index].texture = XMFLOAT2(0.0f, 0.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Bottom right.
        m_vertices[index].position = XMFLOAT3(positionX + m_particleSize, (positionY - (m_particleSize * 16)), positionZ);
        m_vertices[index].texture = XMFLOAT2(1.0f, 1.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Bottom right.
        m_vertices[index].position = XMFLOAT3(positionX + m_particleSize, (positionY - (m_particleSize * 16)), positionZ);
        m_vertices[index].texture = XMFLOAT2(1.0f, 1.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Top left.
        m_vertices[index].position = XMFLOAT3(positionX - m_particleSize, (positionY + (m_particleSize * 16)), positionZ);
        m_vertices[index].texture = XMFLOAT2(0.0f, 0.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Top right.
        m_vertices[index].position = XMFLOAT3(positionX + m_particleSize, (positionY + (m_particleSize * 16)), positionZ);
        m_vertices[index].texture = XMFLOAT2(1.0f, 0.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        //fire uses basic square shape but with a large particle size

        m_particleSize = 0.20f;
        // Bottom left.
        m_vertices[index].position = XMFLOAT3(positionX - m_particleSize, (positionY - (m_particleSize * 1)), positionZ);
        m_vertices[index].texture = XMFLOAT2(0.0f, 1.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Top left.
        m_vertices[index].position = XMFLOAT3(positionX - m_particleSize, (positionY + (m_particleSize * 1)), positionZ);
        m_vertices[index].texture = XMFLOAT2(0.0f, 0.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Bottom right.
        m_vertices[index].position = XMFLOAT3(positionX + m_particleSize, (positionY - (m_particleSize * 1)), positionZ);
        m_vertices[index].texture = XMFLOAT2(1.0f, 1.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Bottom right.
        m_vertices[index].position = XMFLOAT3(positionX + m_particleSize, (positionY - (m_particleSize * 1)), positionZ);
        m_vertices[index].texture = XMFLOAT2(1.0f, 1.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Top left.
        m_vertices[index].position = XMFLOAT3(positionX - m_particleSize, (positionY + (m_particleSize * 1)), positionZ);
        m_vertices[index].texture = XMFLOAT2(0.0f, 0.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

        // Top right.
        m_vertices[index].position = XMFLOAT3(positionX + m_particleSize, (positionY + (m_particleSize * 1)), positionZ);
        m_vertices[index].texture = XMFLOAT2(1.0f, 0.0f);
        m_vertices[index].color = XMFLOAT4(red, green, blue, 1.0f);
        index++;

    // Vertex buffer description
    vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_vertexCount;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vertexBufferDesc.MiscFlags = 0;
    vertexBufferDesc.StructureByteStride = 0;

    // Give the subresource structure a pointer to the vertex data.
    vertexData.pSysMem = m_vertices;
    vertexData.SysMemPitch = 0;
    vertexData.SysMemSlicePitch = 0;

    result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
    if (FAILED(result))
    {
        return false;
    }

    // Static index buffer description
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;
    indexBufferDesc.StructureByteStride = 0;

    // Give the subresource structure a pointer to the index data.
    indexData.pSysMem = indices;
    indexData.SysMemPitch = 0;
    indexData.SysMemSlicePitch = 0;

    result = device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
    if (FAILED(result))
    {
        return false;
    }

    delete[] indices;
    indices = 0;

    //set instance total count to the max number of particles
    m_totalInstanceCount = m_maxParticles;

    // Create the instance array.
    m_Instances = new InstanceType[m_totalInstanceCount];
    if (!m_Instances)
    {
        return false;
    }

    // Initialize vertex array to zeros at first.
    memset(m_Instances, 0, (sizeof(InstanceType) * m_totalInstanceCount));

    // instance buffer description
    instanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    instanceBufferDesc.ByteWidth = sizeof(InstanceType) * m_totalInstanceCount;
    instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    instanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    instanceBufferDesc.MiscFlags = 0;
    instanceBufferDesc.StructureByteStride = 0;

    instanceData.pSysMem = m_Instances;
    instanceData.SysMemPitch = 0;
    instanceData.SysMemSlicePitch = 0;

    result = device->CreateBuffer(&instanceBufferDesc, &instanceData, &m_instanceBuffer);
    if (FAILED(result))
    {
        return false;
    }
    return true;
}


void ParticleManager::ShutdownBuffers()
{
    if (m_instanceBuffer)
    {
        m_instanceBuffer->Release();
        m_instanceBuffer = 0;
    }

    if (m_indexBuffer)
    {
        m_indexBuffer->Release();
        m_indexBuffer = 0;
    }

    if (m_vertexBuffer)
    {
        m_vertexBuffer->Release();
        m_vertexBuffer = 0;
    }

    return;
}

void ParticleManager::UpdateParticles(float frameTime)
{
    Particle* currentNode = m_headOfAllocatedList;

    while (currentNode)
    {
        //if particle is off the ground, increase y velocity by gravity constant
        if (currentNode->positionY > 0.0f)
        {
            currentNode->velocityY += (m_gravityConstant * frameTime);
        }

        //update particles position due to velocity
        MoveParticles(frameTime, currentNode);
        currentNode->remainingLifeTime -= frameTime;

        if (currentNode->positionY < 0.0f)
        {
            currentNode->positionY = 0.1f;

            // change particle velocity to simulate bouncing off the ground, with some reduction in non-veritcal velocity
            currentNode->velocityY = (currentNode->velocityY * -0.4f);
            currentNode->velocityX = (currentNode->velocityX * 0.6f);
            currentNode->velocityZ = (currentNode->velocityZ * 0.6f);
        }

        currentNode = currentNode->next;
    }

    //update rain
    currentNode = m_headOfRainAllocatedList;
    while (currentNode)
    {
        currentNode->velocityY += (m_gravityConstant * frameTime);
        //update particles position due to velocity
        MoveParticles(frameTime, currentNode);
        currentNode = currentNode->next;
    }

    //update fire
    currentNode = m_headOfFireAllocatedList;
    while (currentNode)
    {
        //note that fire particles are not affected by gravity constant

        //update particles position due to velocity
        MoveParticles(frameTime, currentNode);
        currentNode->remainingLifeTime -= frameTime;
        currentNode = currentNode->next;
    }
}

void ParticleManager::MoveParticles(float frameTime, Particle* currentNode)
{
    if (currentNode)
    {
        currentNode->positionX = currentNode->positionX + (currentNode->velocityX * frameTime);
        currentNode->positionY = currentNode->positionY + (currentNode->velocityY * frameTime);
        currentNode->positionZ = currentNode->positionZ + (currentNode->velocityZ * frameTime);
    }
}


void ParticleManager::KillParticles()
{
    //iterate through the allocated list and check for those to kill
    //if kill, delete from allocated list, add to front of free list
    Particle* currentNode = m_headOfAllocatedList;
    Particle* tempNode = nullptr;

    //check head of node last to avoid the need to reassign head node
    if (currentNode)
    {
        while (currentNode->next)
        {

            if (currentNode->next->remainingLifeTime < 0.0f)
            {
                tempNode = currentNode->next;
                currentNode->next = tempNode->next;

                //place in front of of free list
                tempNode->next = m_headOfFreeList;
                m_headOfFreeList = tempNode;
                //No need to iterate to the next node its already at currentNode->next
                continue;
            }
            currentNode = currentNode->next;
        }

        //check head of allocated
        if (m_headOfAllocatedList->remainingLifeTime < 0.0f)
        {
            tempNode = m_headOfAllocatedList;
            m_headOfAllocatedList = m_headOfAllocatedList->next;

            tempNode->next = m_headOfFreeList;
            m_headOfFreeList = tempNode;
        }
    }

    //Reset Rain Particles that hit the ground
    currentNode = m_headOfRainAllocatedList;
    tempNode = nullptr;

    if (currentNode)
    {
        while (currentNode)
        {   // reset all rain particles that are at the ground or lower
            if (currentNode->positionY < 0.0f)
            {
                //create a ring effect to simulate splash particles
                MakeRingEffect(XMFLOAT3(currentNode->positionX, 0.0f, currentNode->positionZ), 8);

                currentNode->positionY = m_rainSpawnInHeight;
                currentNode->velocityY = m_rainSpawnYVelocity;
                continue;
            }
            currentNode = currentNode->next;
        }
    }
    //old Fire gets turned into smoke, old smoke gets deleted and added to the free list
    currentNode = m_headOfFireAllocatedList;
    tempNode = nullptr;
    if (currentNode)
    {
        while (currentNode->next)
        {
            if (currentNode->next->remainingLifeTime < 0.0f)
            {
                tempNode = currentNode->next;
                currentNode->next = tempNode->next;

                if (m_headOfFreeList)
                {
                    tempNode->next = m_headOfFreeList->next;
                    m_headOfFreeList->next = tempNode;
                }
                else
                {
                    tempNode->next = nullptr;
                    m_headOfFreeList = tempNode;
                }


                m_fireInstanceCount--;
                continue;
            } 
            else if (currentNode->next->remainingLifeTime < m_smokeLifeTime)
            {// turn to smoke
                currentNode->next->red = 0.1f;
                currentNode->next->green = 0.1f;
                currentNode->next->blue = 0.1f;
            }
            currentNode = currentNode->next;
        }
        //check head of allocated
        if (m_headOfFireAllocatedList->remainingLifeTime < 0.0f)
        {
            tempNode = m_headOfFireAllocatedList;
            m_headOfFireAllocatedList = m_headOfFireAllocatedList->next;

            if (m_headOfFreeList)
            {
                tempNode->next = m_headOfFreeList->next;
                m_headOfFreeList->next = tempNode;
            }
            else
            {
                tempNode->next = nullptr;
                m_headOfFreeList = tempNode;
            }
            m_fireInstanceCount--;
        } 
        else if (m_headOfFireAllocatedList->remainingLifeTime < m_smokeLifeTime)
        {// turn to smoke
            m_headOfFireAllocatedList->red = 0.1f;
            m_headOfFireAllocatedList->green = 0.1f;
            m_headOfFireAllocatedList->blue = 0.1f;
        }
    }
    return;
}


bool ParticleManager::UpdateBuffers(ID3D11DeviceContext* deviceContext)
{
    int index = 0;
    HRESULT result;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    InstanceType* instanceptr;

    // Initialize vertex array to zeros
    memset(m_Instances, 0, (sizeof(InstanceType) * m_totalInstanceCount));
   
    //rain updates
    auto currentNode = m_headOfRainAllocatedList;
    while (currentNode)
    {
        m_Instances[index].position = XMFLOAT3(currentNode->positionX, currentNode->positionY, currentNode->positionZ);
        m_Instances[index].color = XMFLOAT4(currentNode->red, currentNode->green, currentNode->blue, 1.0f);
        currentNode = currentNode->next;
        index++;
    }

    //Fire updates
    index = m_rainInstanceCount;
    currentNode = m_headOfFireAllocatedList;
    while (currentNode)
    {
        m_Instances[index].position = XMFLOAT3(currentNode->positionX, currentNode->positionY, currentNode->positionZ);
        m_Instances[index].color = XMFLOAT4(currentNode->red, currentNode->green, currentNode->blue, 1.0f);
        currentNode = currentNode->next;
        index++;
    }

    //general update
    index = m_rainInstanceCount + m_fireInstanceCount;

    currentNode = m_headOfAllocatedList;
    while (currentNode)
    {
        m_Instances[index].position = XMFLOAT3(currentNode->positionX, currentNode->positionY, currentNode->positionZ);
        m_Instances[index].color = XMFLOAT4(currentNode->red, currentNode->green, currentNode->blue, 1.0f);
            currentNode = currentNode->next;
            index++;
    }
    m_activeParticles = index;

    // Lock the vertex buffer.
    result = deviceContext->Map(m_instanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return false;
    }

    instanceptr = (InstanceType*)mappedResource.pData;
    memcpy(instanceptr, (void*)m_Instances, (sizeof(InstanceType) * m_totalInstanceCount));

    deviceContext->Unmap(m_instanceBuffer, 0);
    return true;
}


void ParticleManager::RenderBuffers(ID3D11DeviceContext* deviceContext)
{
    unsigned int stride;
    unsigned int offset;
    unsigned int strides[2];
    unsigned int offsets[2];
    ID3D11Buffer* bufferPointers[2];

    // Set the buffer strides.
    strides[0] = sizeof(VertexType);
    strides[1] = sizeof(InstanceType);

    // Set the buffer offsets.
    offsets[0] = 0;
    offsets[1] = 0;

    // Set the array of pointers to the vertex and instance buffers.
    bufferPointers[0] = m_vertexBuffer;
    bufferPointers[1] = m_instanceBuffer;
    // Set the index buffer to active in the input assembler so it can be rendered.
    deviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // Set the vertex buffer to active in the input assembler so it can be rendered.
    deviceContext->IASetVertexBuffers(0, 2, bufferPointers, strides, offsets);

    // Set the type of primitive that should be rendered from this vertex buffer.
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return;
}

void ParticleManager::MakeRingEffect(XMFLOAT3 targetPosition, int numberOfParticles)
{
    bool found;
    float velocityX, velocityZ;

    float circlePosition = 0.0f;
    float radianToDegreeConstant = (3.141592 / 180);

    Particle* currentNode = m_headOfAllocatedList;
    Particle* tempNode = nullptr;

    float OverallVelocity = 0.35f;
    float LifeTime = 0.5f;
    float red = 0.5f;
    float green = 0.5f;
    float blue = 1.0f;

    for (auto i = 0; i < numberOfParticles; i++)
    {
        //calculate the particle's position in the circle
        circlePosition = (360 * ((float)(i) / numberOfParticles));

        velocityX = (OverallVelocity * cos(circlePosition * radianToDegreeConstant));
        velocityZ = (OverallVelocity * sin(circlePosition  * radianToDegreeConstant));

        found = false;
        //find first free particle
        if (m_headOfFreeList != nullptr)
        {
            // break off the head of the free list
            tempNode = m_headOfFreeList;
            m_headOfFreeList = m_headOfFreeList->next;
            //pre set it to the data we want for the new node
            tempNode->next == nullptr;
            tempNode->positionX = targetPosition.x;
            tempNode->positionY = targetPosition.y;
            tempNode->positionZ = targetPosition.z;
            tempNode->red = red;
            tempNode->green = green;
            tempNode->blue = blue;
            tempNode->remainingLifeTime = LifeTime;
            tempNode->velocityX = velocityX;
            tempNode->velocityY = 0.0f;
            tempNode->velocityZ = velocityZ;

            PlaceNodeInZSortedList(tempNode, &m_headOfAllocatedList);
        }
        else
        {
            //no more free particles
            return;
        }
    }
    return;
}

void ParticleManager::MakeFireEffect(XMFLOAT3 targetPosition, float frameTime)
{
    bool found;
    float positionX, positionY, positionZ ;

    float velocityX = 0.0f;
    float velocityY = 1.5f;
    float velocityZ = 0.0f;

    float baseLifeTime = 6.0f;
    float lifeTime = baseLifeTime; // used to hold random lifetime

    float red = 2.0f;
    float green = 0.8f;
    float blue = 0.1f;

    Particle* currentNode = m_headOfFireAllocatedList;
    Particle* tempNode = nullptr;

    //calculate total fire particles to emit this frame
    int TotalFireEffects = (int)(m_fireParticlesPerSecond * frameTime);

    for (auto i = 0; i < TotalFireEffects; ++i)
    {
        // x and z coordiantes are randomized in an area to give the fire depth and width
        positionX = targetPosition.x + (0.04f * (RNGClass::GetRandomInteger(0, 20)));
        positionY = targetPosition.y;
        positionZ = targetPosition.z - (0.0125f * (RNGClass::GetRandomInteger(0, 80)));

        //velocityX is set to a random range to give the fire a cone shape as the particles rise
        velocityX = (0.015f * (RNGClass::GetRandomInteger(-10, 10)));

        //randomized additional lifetime for each particle gives the top of the fire a flickering effect
        lifeTime = baseLifeTime + (0.1 * RNGClass::GetRandomInteger(0, 15));

        found = false;
        //find first free particle
        if (m_headOfFreeList != nullptr)
        {
            // break off the head of the free list
            tempNode = m_headOfFreeList;
            m_headOfFreeList = m_headOfFreeList->next;
            //pre set it to the data we want for the new node
            tempNode->next == nullptr;
            tempNode->positionX = positionX;
            tempNode->positionY = positionY;
            tempNode->positionZ = positionZ;
            tempNode->red = red;
            tempNode->green = green;
            tempNode->blue = blue;
            tempNode->remainingLifeTime = lifeTime;
            tempNode->velocityX = velocityX;
            tempNode->velocityY = velocityY;
            tempNode->velocityZ = velocityZ;

            PlaceNodeInZSortedList(tempNode, &m_headOfFireAllocatedList);
            m_fireInstanceCount++;
        }
        else
        {
            //no more free particles
            return;
        }
    }
}

void ParticleManager::InitiateRainEffects()
{
    bool found;
    float positionX, positionY, positionZ, red, green, blue;

    float velocityX = 0.0f;
    float velocityY = m_rainSpawnYVelocity;
    float velocityZ = 0.0f;

    //rain is not affected by lifetime, it is deleted when it is below or at ground level
    float lifeTime = 0.0f;

    Particle* currentNode = m_headOfRainAllocatedList;
    Particle* tempNode = nullptr;

    red =  0.5f;
    green = 0.5f;
    blue = 1.0f;

    float rainXDifferential = 0;
    float rainYDifferential = 0;
    float rainZDifferential = 0;

    for (auto i = 0; i < m_rainInstanceCount; i++)
    {
        //y differential is simply a height between the spawn height and the ground
        // x and z differential is a random integer between 0 and the range of the specific coordiante multiplied by a factor of 10 to give more diversity
        rainXDifferential = (0.1 * (RNGClass::GetRandomInteger(0, (int)(m_rainBoxCoordinates[1] - m_rainBoxCoordinates[0]) * 10)));
        rainYDifferential = (0.1 * (RNGClass::GetRandomInteger(0, (int)m_rainSpawnInHeight * 10)));
        rainZDifferential = (0.1 * (RNGClass::GetRandomInteger(0, (int)(m_rainBoxCoordinates[3] - m_rainBoxCoordinates[2])  * 10)));
        positionX = m_rainBoxCoordinates[0] + rainXDifferential;
        positionY = m_rainSpawnInHeight - rainYDifferential;
        positionZ = m_rainBoxCoordinates[2] + rainZDifferential;


        found = false;
        //find first free list
        if (m_headOfFreeList != nullptr)
        {
            tempNode = m_headOfFreeList;
            m_headOfFreeList = m_headOfFreeList->next;

            tempNode->next == nullptr;
            tempNode->positionX = positionX;
            tempNode->positionY = positionY;
            tempNode->positionZ = positionZ;
            tempNode->red = red;
            tempNode->green = green;
            tempNode->blue = blue;
            tempNode->remainingLifeTime = lifeTime;
            tempNode->velocityX = velocityX;
            tempNode->velocityY = velocityY;
            tempNode->velocityZ = velocityZ;

            PlaceNodeInZSortedList(tempNode, &m_headOfRainAllocatedList);

        }
        else
        {//no more free particles
            return;
        }
    }
}

void ParticleManager::PlaceNodeInZSortedList(Particle * insertNode, Particle **headNode)
{
    bool found = false;
    Particle* currentNode = (*headNode);
    Particle* tempNode = nullptr;

    //if the headnode is nullptr then the list is empty and we can place our node as the head
    if (currentNode == nullptr)
    {
        (*headNode) = insertNode;
        (*headNode)->next = nullptr;
        currentNode = (*headNode);
        return;
    }
    // iterate through the list to find the appropirate position, if end of list is found, place at the end
    while (!found)
    {
        if (currentNode->next == nullptr)
        {
            found = true;
            insertNode->next = nullptr;
            currentNode->next = insertNode;
        }
        else if (currentNode->next->positionZ < insertNode->positionZ)
        {
            found = true;
            insertNode->next = currentNode->next;
            currentNode->next = insertNode;
        }
        currentNode = currentNode->next;
    }
    return;
}
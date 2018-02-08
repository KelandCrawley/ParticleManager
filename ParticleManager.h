#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <math.h>

#include "RNGClass.h"
#include "TextureClass.h"

using namespace DirectX;

class ParticleManager
{
private:
        struct Particle
        {
            float positionX, positionY, positionZ;
            float red, green, blue, alpha;
            float velocityX, velocityY, velocityZ;

            Particle* next;
            float remainingLifeTime;
        };
    //per instnace data
    struct InstanceType
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
    };
    // vertex data
    struct VertexType
    {
        XMFLOAT3 position;
        XMFLOAT2 texture;
        XMFLOAT4 color;
    };
    
    //linked lists
    Particle* m_headOfAllocatedList;
    Particle* m_headOfRainAllocatedList;
    Particle* m_headOfFreeList;
    Particle* m_headOfFireAllocatedList;

public:
    ParticleManager();
    ~ParticleManager();

    // calls private load/initilize functions for textures as well as vertex and instance buffers
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* defaultTextureFilename, const char* rainTextureFilename, const char* fireTextureFilename);
    void Shutdown();

    bool Frame(ID3D11DeviceContext* deviceContext, float frameTime);
    void Render(ID3D11DeviceContext* deviceContext);

    //standard getters

    ID3D11ShaderResourceView* GetDefaultTexture();
    ID3D11ShaderResourceView* GetRainTexture();
    ID3D11ShaderResourceView* GetFireTexture();
    int GetIndexCount();
    int GetVertexCount();
    int GetRainInstanceCount();
    int GetFireInstanceCount();
    int GetTotalInstanceCount();
    int GetActiveInstanceCount();

private:

    //sets texture pointer to a new texture and then initializes it using the file passed in
    bool LoadTexture(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* filename, TextureClass** texture);
    void ReleaseTextures();

    TextureClass* m_defaultTexture;
    TextureClass* m_rainTexture;
    TextureClass* m_fireTexture;

    //particle initialize
    bool InitializeParticleSystem();
    void ShutdownParticleSystem();

    bool InitializeBuffers(ID3D11Device* device);
    void ShutdownBuffers();


    void UpdateParticles(float frameTime);
    //moves particles based on thier velocity
    void MoveParticles(float frameTime, Particle *currentNode);
    void KillParticles();

    //Updates the instance buffers with the individual instance data for each particle type
    bool UpdateBuffers(ID3D11DeviceContext* deviceContext);
    // set the stride/offest and set the buffers
    void RenderBuffers(ID3D11DeviceContext* deviceContext);



    //m_maxParticles will be the number of particles allocated by the memory manager
    int m_maxParticles;
    float m_gravityConstant;
    float m_particleSize;

    //array backing of the particle lists
    Particle* m_particleList;

    int m_vertexCount, m_indexCount;
    VertexType* m_vertices;
    InstanceType* m_Instances;
    ID3D11Buffer *m_vertexBuffer, *m_indexBuffer, *m_instanceBuffer;
    int m_totalInstanceCount, m_rainInstanceCount, m_fireInstanceCount;
    int m_fireParticlesPerSecond;
    float m_rainSpawnInHeight, m_rainSpawnYVelocity;
    float m_smokeLifeTime;
    float m_activeParticles;

    // (xleft boundry, xright boundry, zclose boundry, zfar boundry)
    float m_rainBoxCoordinates[4];


    //Particle effects, 
    // makes a ring effect at given posotin, in the demo it is used in the rain spash effect
    void MakeRingEffect(XMFLOAT3, int numberOfParticles);

    //makes a number of fire particles at a given position, the number of partilces is equal to m_fireParticlesPerSecond / frametime
    void MakeFireEffect(XMFLOAT3 targetPosition, float frameTime);

    //starts the rainfall particles on their way, the kill function restarts the rain partciles, so this funciton needs only be called to start the effect
    void InitiateRainEffects();

    //linked list helper functins

    //places a node into its appropriate list, ordered by z position for alpha blending purposes
    //@param insertNode: is the node of the from the free list that is already loaded with the proper data and ready to be put in the list
    //@param headNode: The head of the list that you want the node inserted into
    void PlaceNodeInZSortedList(Particle* insertNode, Particle** headNode);
};


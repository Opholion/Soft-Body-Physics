//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------

#include "Scene.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
/*
    ImGui::Begin("Particle Controls", 0, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::InputInt("Num Particles", &showSpringNum, 1, gCubeMesh[0]->getSpringSize());
    ImGui::End();
*/

//--------------------------------------------------------------------------------------
// Scene Data
//--------------------------------------------------------------------------------------
// Addition of Mesh, Model and Camera classes have greatly simplified this section
// Geometry data has gone to Mesh class. Positions, rotations, matrices have gone to Model and Camera classes

// Constants controlling speed of movement/rotation (measured in units per second because we're using frame time)
const float ROTATION_SPEED = 2.0f;  // 2 radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // 50 units per second for movement (what a unit of length is depends on 3D model - i.e. an artist decision usually)

static bool go = true;
static bool isGravity = false;

static float totalFrameTime = 0;
static int frameCount = 0;

constexpr int DELAY_SPRINGS = true;

constexpr float NodeScale = 0.0085f;
constexpr float SpringScale = 0.1; //Cubes have a size of (10,10,10)
//--------------------------------------------------------------------------------------
//**** Shadow Texture  ****//
//--------------------------------------------------------------------------------------
// This texture will have the scene from the point of view of the light renderered on it. This texture is then used for shadow mapping

// Dimensions of shadow map texture - controls quality of shadows
const int gShadowMapSize = 256 * 10; //Needs to be a multiple of 256. This is faster to calculate (for me.)


//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
// Variables sent over to the GPU each frame
// The structures are now in Common.h
// IMPORTANT: Any new data you add in C++ code (CPU-side) is not automatically available to the GPU
//            Anything the shaders need (per-frame or per-model) needs to be sent via a constant buffer

PerFrameConstants gPerFrameConstants;      // The constants that need to be sent to the GPU each frame (see common.h for structure)
ID3D11Buffer* gPerFrameConstantBuffer; // The GPU buffer that will recieve the constants above

PerModelConstants gPerModelConstants;      // As above, but constant that change per-model (e.g. world matrix)
ID3D11Buffer* gPerModelConstantBuffer; 


bool SceneManager::InitGeometry()
{
    // Load mesh geometry data, just like TL-Engine this doesn't create anything in the scene. Create a Model for that.
    // IMPORTANT NOTE: Will only keep the first object from the mesh - multipart objects will have parts missing - see later lab for more robust loader


    try
    { //HoverTank01 cat_01_color05.FBX"
        //If the secondary value is true then it will need to be unique for each mesh. 
        gSoftBodyMesh[0] = new Mesh("cat_01_color05.FBX", false, true);
        gSoftBodyMesh[1] = new Mesh("Cube.x", false, true);

        //If the secondary value is true then it will need to be unique for each mesh. 
        gSoftBodyMesh[2] = new Mesh("Sphere.x", false, true);
        gSoftBodyMesh[3] = new Mesh("Sphere.x", false, true);

        //If the secondary value is true then it will need to be unique for each mesh. 
        gSoftBodyMesh[4] = new Mesh("HoverTank01.x", false, true);
        gSoftBodyMesh[5] = new Mesh("HoverTank01.x", false, true);

        //Floor isn't actually interacted with. Currently the softbodies have a 'ground' limit as colliding with the floor is expensive and can stress test its potential to err.
        //Called boundary as it acts as the boundary to different areas.
        gFloorMesh = new Mesh("Floor.x");
        gBoundaryMesh = new Mesh("Cube.x");


        gSpringMesh = new Mesh("Cube.x");
        gLightMesh = new Mesh("Light.x");
    }
    catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
    {
        gLastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
        return false;
    }


    // Load the shaders required for the geometry we will use (see Shader.cpp    / .h)
    if (!LoadShaders())
    {
        gLastError = "Error loading shaders";
        return false;
    }


    // Create GPU-side constant buffers to receive the gPerFrameConstants and gPerModelConstants structures above
    // These allow us to pass data from CPU to shaders such as lighting information or matrices
    // See the comments above where these variable are declared and also the UpdateScene function
    gPerFrameConstantBuffer = CreateConstantBuffer(sizeof(gPerFrameConstants));
    gPerModelConstantBuffer = CreateConstantBuffer(sizeof(gPerModelConstants));
    if (gPerFrameConstantBuffer == nullptr || gPerModelConstantBuffer == nullptr)
    {
        gLastError = "Error creating constant buffers";
        return false;
    }

    //Cube map texture
    if ((HRESULT)DirectX::CreateDDSTextureFromFileEx
    (
        gD3DDevice, //Engine/Device
        L"StreetCubeMap.dds", //Filename
        2048 * 2048, //Size of texture
        D3D11_USAGE_DEFAULT, //Usage
        D3D11_BIND_SHADER_RESOURCE, //Bindflags
        NULL, //CPUaccess
        D3D11_RESOURCE_MISC_TEXTURECUBE, //MiscFlags, tells it that it's used for cubemapping
        false,
        (ID3D11Resource**)&gStaticReflectionCubeMap,
        &gStaticReflectionCubeMapSRV
    ) != S_OK)
    {
        gLastError = "Error loading cubemap textures";
        return false;
    }
    // Load / prepare textures on the GPU ////
    // Load textures and create DirectX objects for them


    for (int i = 0; i < CUBETEXTURECOUNT; ++i)
    {
        if (!LoadTexture(ARR_CUBETEXTURES[i], &gCubeDiffuseSpecularMap[i], &gCubeDiffuseSpecularMapSRV[i]))
        {
            gLastError = "Error loading cube textures: " + ARR_CUBETEXTURES[i];
            return false;
        }
    }

    //**** Create Shadow Map texture ****//

    // We also need a depth buffer to go with our portal
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = gShadowMapSize; // Size of the shadow map determines quality / resolution of shadows
    textureDesc.Height = gShadowMapSize;
    textureDesc.MipLevels = 1; // 1 level, means just the main texture, no additional mip-maps. Usually don't use mip-maps when rendering to textures (or we would have to render every level)
    textureDesc.ArraySize = 2;
    textureDesc.Format = DXGI_FORMAT_R32_TYPELESS; // The shadow map contains a single 32-bit value [tech gotcha: have to say typeless because depth buffer and shaders see things slightly differently]
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE; // Indicate we will use texture as a depth buffer and also pass it to shaders
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    for (int i = 0; i < SPOT_LIGHT_SHADOW_MAP_COUNT; ++i)
    {
        if (FAILED(gD3DDevice->CreateTexture2D(&textureDesc, NULL, &gSpotShadowMap[i].Texture)))
        {
            gLastError = "Error creating shadow map texture " + i;
            return false;
        }
    }


    // Create the depth stencil view, i.e. indicate that the texture just created is to be used as a depth buffer
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // See "tech gotcha" above. The depth buffer sees each pixel as a "depth" float
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    dsvDesc.Flags = 0;


    for (int i = 0; i < SPOT_LIGHT_SHADOW_MAP_COUNT; ++i)
    {
        if ((FAILED(gD3DDevice->CreateDepthStencilView(gSpotShadowMap[i].Texture, &dsvDesc, &gSpotShadowMap[i].DepthStencil))))
        {
            gLastError = "Error creating shadow map depth stencil view: 1";
            return false;
        }
    }


    // We also need to send this texture (resource) to the shaders. To do that we must create a shader-resource "view"
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // See "tech gotcha" above. The shaders see textures as colours, so shadow map pixels are not seen as depths
                                           // but rather as "red" floats (one float taken from RGB). Although the shader code will use the value as a depth
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    for (int i = 0; i < SPOT_LIGHT_SHADOW_MAP_COUNT; ++i)
    {
        if (FAILED(gD3DDevice->CreateShaderResourceView(gSpotShadowMap[i].Texture, &srvDesc, &gSpotShadowMap[i].ShaderResourceView)))
        {
            gLastError = "Error creating shadow map shader resource view";
            return false;
        }
    }


    //*****************************//

     // Create all filtering modes, blending modes etc. used by the app (see State.cpp/.h)
    if (!CreateStates())
    {
        gLastError = "Error creating states";
        return false;
    }

    //test();

    return true;
}




// Prepare the scene
// Returns true on success
bool SceneManager::InitScene()
{
    gGround = new Model(gFloorMesh);


    for (int j = 0; j < ARR_SCENE_COUNT * ARR_SOFT_BODY_COUNT; ++j)
    {
        gSoftBody[j] = new Model(gSoftBodyMesh[j]);
    }

    unsigned int VertexloopLimit = gSoftBody[0]->GetVectorMax();
    for (int i = 0; i < /*50; ++i)*/VertexloopLimit; ++i)
    {
        VectorRep.push_back(new Model(gSpringMesh));
    }

    unsigned int SpringloopLimit = gSoftBodyMesh[0]->getSpringSize();
    for (int i = 0; i < /*50; ++i)*/SpringloopLimit; ++i)
    {
        SpringRep.push_back(new Model(gSpringMesh));
    }


    // Initial positions
    for (int j = 0; j < ARR_SCENE_COUNT * ARR_SOFT_BODY_COUNT; j += ARR_SOFT_BODY_COUNT)
    {
        for (int i = 0; i < ARR_SOFT_BODY_COUNT; ++i)
        {
            gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + j+i]->SetPosition({ 30.0f + (i * 25.0f), 50.0f, 10.0f });///(-20 * (arrCubeSize / 2)) + 20 * (float)i });
            gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + j+i]->SetScale(0.999f); //Set slightly smaller to allow springs to peak through more.
        }
    }

    //Node positions given visual representation.
    for (int i = 0; i < VectorRep.size(); ++i)
    {
        VectorRep[i]->SetPosition(CVector3(0.0f, 0.0f, 0.0f));
        VectorRep[i]->SetScale(NodeScale);
    }
    //Set the springs to a basic position, ready to recieve data.
    for (int i = 0; i < SpringRep.size(); ++i)
    {
        SpringRep[i]->SetPosition(CVector3(0.0f, 0.0f, 0.0f));
    }
    showSpringNum = SpringRep.size();

    // Light set-up - using an array this time
    UnqPtr_Lights[0]->ModelSetup(0, gLightMesh, lightStartPositions[0], CVector3(.0f, .0f, .0f));
    for (int i = 1; i < LIGHT_COUNTER; ++i)
    {
        UnqPtr_Lights[i]->ModelSetup(i, gLightMesh, lightStartPositions[i], worldCenter);
    }

    //// Set up camera ////
    gCamera = new Camera();
    gCamera->SetPosition({ 15, 30,-70 });
    gCamera->SetRotation({ ToRadians(13), 0, 0 });

    return true;
}


// Release the geometry and scene resources created above
void SceneManager::ReleaseResources()
{
    ReleaseStates();

    for (int i = 0; i < SPOT_LIGHT_SHADOW_MAP_COUNT; ++i)
    {
        if (gSpotShadowMap[i].DepthStencil)  gSpotShadowMap[i].DepthStencil->Release();
        if (gSpotShadowMap[i].ShaderResourceView)           gSpotShadowMap[i].ShaderResourceView->Release();
        if (gSpotShadowMap[i].Texture)       gSpotShadowMap[i].Texture->Release();
    }

    if (gStaticReflectionCubeMap)             gStaticReflectionCubeMap->Release();
    if (gStaticReflectionCubeMapSRV)             gStaticReflectionCubeMapSRV->Release();

    for (int i = 0; i < CUBETEXTURECOUNT; ++i)
    {
        if (gCubeDiffuseSpecularMap[i])    gCubeDiffuseSpecularMap[i]->Release();
        if (gCubeDiffuseSpecularMapSRV[i]) gCubeDiffuseSpecularMapSRV[i]->Release();
    }


    if (gPerModelConstantBuffer)  gPerModelConstantBuffer->Release();
    if (gPerFrameConstantBuffer)  gPerFrameConstantBuffer->Release();

    ReleaseShaders();

    delete gCamera;    gCamera = nullptr;


    for (int i = 0; i < ARR_SCENE_COUNT*ARR_SOFT_BODY_COUNT; ++i)
    {
        delete gSoftBodyMesh[i]; gSoftBodyMesh[i] = nullptr;
    }
    delete gGround;    gGround = nullptr;
    delete gLightMesh;     gLightMesh = nullptr;
    delete gFloorMesh;    gFloorMesh = nullptr;
    delete gSpringMesh;     gSpringMesh = nullptr;
}



//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------

// Render the scene from the given light's point of view. Only renders depth buffer
void SceneManager::RenderDepthBufferForLightIndex(int lightIndex)
{


    // Get camera-like matrices from the spotlight, seet in the constant buffer and send over to GPU
    gPerFrameConstants.viewMatrix = UnqPtr_Lights[lightIndex]->CalculateViewMatrix();
    gPerFrameConstants.projectionMatrix = UnqPtr_Lights[lightIndex]->CalculateProjectionMatrix();
    gPerFrameConstants.viewProjectionMatrix = gPerFrameConstants.viewMatrix * gPerFrameConstants.projectionMatrix;
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    // Indicate that the constant buffer we just updated is for use in the vertex shader (VS) and pixel shader (PS)
    gD3DContext->PSSetConstantBuffers(0, 2, &gPerFrameConstantBuffer);
    gD3DContext->VSSetConstantBuffers(0, 2, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 


    //// Only render models that cast shadows ////

    // Use special depth-only rendering shaders
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gDepthOnlyPixelShader, nullptr, 0);

    // States - no blending, normal depth buffer and culling
    gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullBackState); //I did both.

    // Render models - no state changes required between each object in this situation (no textures used in this step)
    for (int i = 0; i < ARR_SOFT_BODY_COUNT; ++i)
    {
        gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + i]->Render();
    }
}


// Render everything in the scene from the given camera
// This code is common between rendering the main scene and rendering the scene in the portal
void SceneManager::RenderSceneFromCamera(Camera* camera)
{
    // Set camera matrices in the constant buffer and send over to GPU
    gPerFrameConstants.viewMatrix = camera->ViewMatrix();
    gPerFrameConstants.projectionMatrix = camera->ProjectionMatrix();
    gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    // Indicate that the constant buffer we just updated is for use in the vertex shader (VS) and pixel shader (PS)
    gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
    gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);

    //// Render lit models ////


    // Select which shaders to use next
    gD3DContext->VSSetShader(gPixelLightingVertexShader[0], nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader[0], nullptr, 0);

    // States - no blending, normal depth buffer and culling
    gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullBackState);

    // Select the approriate textures and sampler to use in the pixel shader

            // Select which shaders to use next
    gD3DContext->VSSetShader(gPixelLightingVertexShader[2], nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader[2], nullptr, 0);
    //TEXTURE FOR GROUND HEWRE >..................................................
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    gGround->Render();

    // Render model - it will update the model's world matrix and send it to the GPU in a constant buffer, then it will call
    // the Mesh render function, which will set up vertex & index buffer before finally calling Draw on the GPU
    // This will also update the node positions if this is a softbody.




    gD3DContext->PSSetShaderResources(0, 1, &gCubeDiffuseSpecularMapSRV[1]);


    gD3DContext->PSSetSamplers(0, 1, &gPointSampler);
    gD3DContext->VSSetShader(gPixelLightingVertexShader[0], nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader[0], nullptr, 0);


    gD3DContext->PSSetShaderResources(0, 1, &gCubeDiffuseSpecularMapSRV[2]);

    //Render every soft body within the current scene. These soft bodies will appear as rubber
    for (int i = 0; i < ARR_SOFT_BODY_COUNT; ++i)
    {
        if (showSprings < SHOW_SPRINGS_LIMIT || i != 0)
        {
            gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + i]->Render();
        }
    }


    //There does not need to be a texture for the springs as they just output (1,1,1) in the pixel shader to make them more visible.
    gD3DContext->VSSetShader(gPixelLightingVertexShader[1], nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader[1], nullptr, 0);

    //Springs are optionally rendered.
    if (showSprings >= SpringShowingNumbers(2))
    {
        for (int i = 0; i < VectorRep.size(); ++i)
        {
            VectorRep[i]->Render();
        }

        if (showSprings >= SpringShowingNumbers(3))
        {
            for (int i = 0; i < /*50; ++i)*/SpringRep.size(); ++i)
            {
                if (i < showSpringNum)
                {
                    SpringRep[i]->Render();
                }
            }
        }
    }


    gD3DContext->PSSetShaderResources(0, 1, &gCubeDiffuseSpecularMapSRV[0]); // First parameter must match texture slot number in the shade
    gD3DContext->OMSetBlendState(gSubtractiveBlendState, nullptr, 0xffffff);


    //// Render lights ////
    gD3DContext->VSSetShader(gPixelLightingVertexShader[2], nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader[2], nullptr, 0);

    // Select which shaders to use next
    gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gDepthOnlyPixelShader, nullptr, 0);

    // States - additive blending, read-only depth buffer and no culling (standard set-up for blending
    gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    // Render all the lights in the array
    for (int i = 0; i < LIGHT_COUNTER; ++i)
    {
        gPerModelConstants.objectColour = UnqPtr_Lights[i]->getColour(); // Set any per-model constants apart from the world matrix just before calling render (light colour here)
        UnqPtr_Lights[i]->Render();
    }


    //Apply collisions as soon as rendering is done.
    //Iterate through each polygons within every potential colliding model, checking if a point is within its bounds
    if (isCollisionOn)
    {
        for (int i = 0; i < ARR_SOFT_BODY_COUNT; ++i)
        {
            for (int j = 0; j < ARR_SOFT_BODY_COUNT; ++j)
            {
                if (i != j)
                {
                    gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + i]->isCollision(gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + j]);
                }
            }
        }
    }
}



// Rendering the scene now renders everything twice. First it renders the scene for the portal into a texture.
// Then it renders the main scene using the portal texture on a model.
void SceneManager::RenderScene()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();



    //  gCube[0]->
    if (!go)
    {
        if (isGravity)
        {

            gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->VertexData.applyForce(frameTime, CVector3(Cube0momentum.x, Cube0momentum.y + -gravityStrength, Cube0momentum.z));
            gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 1]->VertexData.applyForce(frameTime, CVector3(0, 0 + -gravityStrength, 0));
        }
        else
        {
            gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->VertexData.applyForce(frameTime, Cube0momentum);
            gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 1]->VertexData.applyForce(frameTime, CVector3(0, 0, 0));
        }

    }

    for (int i = 0; i < UnqPtr_Lights.size(); ++i)
    {
        UnqPtr_Lights[i]->LightEffect(frameTime);
    }
    //// Common settings ////

    // Set up the light information in the constant buffer
    // Don't send to the GPU yet, the function RenderSceneFromCamera will do that

    //Colour rotation for light [0]

    for (int i = 0; i < LIGHT_COUNTER; ++i)
    {
        UnqPtr_Lights[i]->LightStatUpdate(i);
        // lightSetup(i);
    }

    gPerFrameConstants.lightDirection = { 15.0f,10.0f,10.0f };
    gPerFrameConstants.ambientColour = gAmbientColour;
    gPerFrameConstants.specularPower = gSpecularPower;
    gPerFrameConstants.cameraPosition = gCamera->Position();

    //***************************************//

    //// Render from light's point of view ////

    // Only rendering from light 1 to begin with

    // Setup the viewport to the size of the shadow map texture
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<FLOAT>(gShadowMapSize);
    vp.Height = static_cast<FLOAT>(gShadowMapSize);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    gD3DContext->RSSetViewports(1, &vp);



    // Select the shadow map texture as the current depth buffer. We will not be rendering any pixel colours
    // Also clear the the shadow map depth buffer to the far distance

    for (int i = 0; i < SPOT_LIGHT_SHADOW_MAP_COUNT; ++i)
    {
        gD3DContext->OMSetRenderTargets(0, nullptr, gSpotShadowMap[i].DepthStencil);
        gD3DContext->ClearDepthStencilView(gSpotShadowMap[i].DepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);
        RenderDepthBufferForLightIndex(i);
    }

    //**************************//

    //// Main scene rendering ////

    // Set the back buffer as the target for rendering and select the main depth buffer.
    // When finished the back buffer is sent to the "front buffer" - which is the monitor.

    gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);


    // Clear the back buffer to a fixed colour and the depth buffer to the far distance
    gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);
    gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);



    // Setup the viewport to the size of the main window
    vp.Width = static_cast<FLOAT>(gViewportWidth);
    vp.Height = static_cast<FLOAT>(gViewportHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    gD3DContext->RSSetViewports(1, &vp);


    // Set shadow maps in shaders
    // First parameter is the "slot", must match the Texture2D declaration in the HLSL code
    // In this app the diffuse map uses slot 0, the shadow maps use slots 1 onwards. If we were using other maps (e.g. normal map) then
    // we might arrange things differently

    for (int i = 0; i < SPOT_LIGHT_SHADOW_MAP_COUNT; ++i)
    {
        gD3DContext->PSSetShaderResources(i + 1, 1, &gSpotShadowMap[i].ShaderResourceView);

        gD3DContext->PSSetSamplers(i + 1, 1, &gPointSampler);
    }

    RenderSceneFromCamera(gCamera);


    // Unbind shadow maps from shaders - prevents warnings from DirectX when we try to render to the shadow maps again next frame
    ID3D11ShaderResourceView* nullView = nullptr;

    for (int i = 1; i < SPOT_LIGHT_SHADOW_MAP_COUNT + 1; ++i)
    {
        gD3DContext->PSSetShaderResources(i, 1, &nullView);
    }


    //Begin ImGui screen

    ImGui::Begin("Soft body Controls - IJKL may be used for movement", 0, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SliderInt("Set gravity strength", &gravityStrength, 10, GRAVITY_LIMIT);
    if (ImGui::Button("Activate gravity"))
    {
        isGravity = !isGravity;
    }
    ImGui::SliderInt("Show model springs/nodes", &showSprings, 1, SHOW_SPRINGS_LIMIT);
    ImGui::SliderInt("Number of springs shown", &showSpringNum, 1, gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->getSpringSize());
    ImGui::InputInt("Strength of model springs (Type toggled below)", &GUIOutputCopy_SpringStrengthModel, 1, 1000);


    ImGui::SliderInt("Affect inner springs", &effectSpringType[0], 0, 1);
    ImGui::SliderInt("Affect inner-to-outer springs", &effectSpringType[1], 0, 1);
    ImGui::SliderInt("Affect outer springs", &effectSpringType[2], 0, 1);


    //Iterate through the springs. Only effecting them based on the sliders involved
    //As the outer nodes are bound in place it makes sense to control how the springs
    //Interact with them uniquely. 
    if (ImGui::Button("Apply spring change"))
    {
        int loopLimitSpring = SpringRep.size();
        for (int i = 0; i < /*50; ++i)*/loopLimitSpring; ++i)
        {
            if (loopLimitSpring > gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->getSpringSize())
            {
                loopLimitSpring = gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->getSpringSize();
            }

            int springType = gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->SpringData[i]->getBoundParentCount();
            if (springType == 0 && effectSpringType[0])
            {
                gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->SetSpringStrength(i, GUIOutputCopy_SpringStrengthModel);
            }
            if (springType == 1 && effectSpringType[1])
            {
                gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->SetSpringStrength(i, GUIOutputCopy_SpringStrengthModel);
            }
            if (springType == 2 && effectSpringType[2])
            {
                gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->SetSpringStrength(i, GUIOutputCopy_SpringStrengthModel);
            }
        }
    }
    ImGui::End();
    


    ImGui::Begin("User Controls", 0, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SliderInt("Change current scene", &currScene, 0, ARR_SCENE_COUNT-1);

    if (ImGui::Button("Reset softbody"))
    {
        Cube0momentum *= 0.0f; //Movement is superficial. Ponts are pushed in a direction and not part of the class itself.
        //Note that without resistance this will result in objects appearing as if they are being "Pushed", thickening on the side the force is applied
        gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 0]->VertexData.resetPoints();
        gSoftBodyMesh[(currScene * ARR_SOFT_BODY_COUNT) + 1]->VertexData.resetPoints();
    }
    if (ImGui::Button("Activate softbody"))
    {
        go = !go;
    }
    if (ImGui::Button("Switch controlled object"))
    {
        SwitchControl = !SwitchControl;
    }
    if (ImGui::Button("Toggle collisions - Unstable"))
    {
        isCollisionOn = !isCollisionOn; //Disabled due to errors
    }
    ImGui::End();


    //showSprings
    //// Scene completion ////
    ImGui::Render();
    gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

//Scene compltetion
    // When drawing to the off-screen back buffer is complete, we "present" the image to the front buffer (the screen)
    gSwapChain->Present(0, 0);
}


//--------------------------------------------------------------------------------------
// Scene Update
//--------------------------------------------------------------------------------------
void SceneManager::RunScene(float frameTime_2)
{
    frameTime = frameTime_2;
   
    UpdateScene();
    RenderScene();
}
// Update models and camera. frameTime is the time passed since the last frame
void SceneManager::UpdateScene()
{



    gPerModelConstants.Wiggle += 1.0f * frameTime;
    // Create a matrix to position the camera - called the view (camera) matrix - we'll see this in more detail later
    gPerFrameConstants.viewMatrix = InverseAffine(MatrixTranslation(CVector3(0, 0, -4.0f)));

    // Create a "projection matrix" - this determines properties of the camera - again we'll see this later
    gPerFrameConstants.projectionMatrix = MakeProjectionMatrix();


    // Control sphere (will update its world matrix)
    if (SwitchControl)
    {

        gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + 1]->Control(frameTime, Key_I, Key_K, Key_J, Key_L, Key_U, Key_O, Key_Period, Key_Comma);

    }
    else {
        if (KeyHeld(Key_I))
        {
            Cube0momentum += CVector3(0, 0, CUBE_SPEED);
        }

        if (KeyHeld(Key_K))
        {
            Cube0momentum += CVector3(0, 0, -CUBE_SPEED);
        }

        if (KeyHeld(Key_J))
        {
            Cube0momentum += CVector3(-CUBE_SPEED, 0, .0f);
        }

        if (KeyHeld(Key_L))
        {
            Cube0momentum += CVector3(CUBE_SPEED, 0, .0f);
        }
    }

    //Activate/Deactive Sphere->Soft-body collision
    CVector3 Base = gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + 0]->Position();
    CVector3 B_Scale = gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + 0]->Scale();
    if (showSprings >= SpringShowingNumbers(2))
    {
        //Iterate through the nodes within a mesh to create a visual representation placement.
        for (int i = 0; i < VectorRep.size(); ++i)
        {
            CVector3 PolygonModifier = gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + 0]->GetCollisionVectors(i);


            VectorRep[i]->SetPosition
            (
                Base +
                CVector3(PolygonModifier.x * B_Scale.x, PolygonModifier.y * B_Scale.y, PolygonModifier.z * B_Scale.z)
            );
        }
    }
    int loopLimitSpring = SpringRep.size();

    //Calculate physical spring representation
    for (int i = 0; i < /*50; ++i)*/loopLimitSpring; ++i)
    {
        if (showSprings >= SpringShowingNumbers(3))
        {
            CVector3 PolygonFaceMod = gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + 0]->getSpringFacing(i, 0);
            CVector3 PolygonModifier = gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + 0]->getSpring(i, 0);//->getSpring(i);
            float ZScaler = Distance(Base + PolygonFaceMod, Base + gSoftBody[(currScene * ARR_SOFT_BODY_COUNT) + 0]->getSpringFacing(i, 1));

            //.0025f
            SpringRep[i]->SetScale(CVector3(NodeScale * 0.1f, NodeScale * 0.1f, ZScaler * SpringScale));
            SpringRep[i]->SetPosition(Base + PolygonModifier);//CVector3(PolygonModifier.x * B_Scale.x, PolygonModifier.y * B_Scale.y, PolygonModifier.z * B_Scale.z)); //Base + PolygonModifier);   
            SpringRep[i]->FaceTarget(Base + PolygonFaceMod);
        }
    }

    // Control camera (will update its view matrix)
    gCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D);


    // Show frame time / FPS in the window title //
    const float fpsUpdateTime = 0.5f; // How long between updates (in seconds)

    totalFrameTime += frameTime;
    ++frameCount;


    // Displays FPS rounded to nearest int, and frame time (more useful for developers) in milliseconds to 2 decimal places
    float avgFrameTime = totalFrameTime / frameCount;
    std::ostringstream frameTimeMs;
    frameTimeMs.precision(2);
    frameTimeMs << std::fixed << avgFrameTime * 1000;

    // std::to_string(frameTime)
    std::string windowTitle = "Oliver Edmondson: Final Project-Springs; Frame Time: " + frameTimeMs.str() +
        "ms, FPS: " + std::to_string(static_cast<int>(1 / avgFrameTime + 0.5f));
    SetWindowTextA(gHWnd, windowTitle.c_str());
    totalFrameTime = 0;
    frameCount = 0;

}

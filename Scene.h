//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------
#include "CVector3.h"
#include <cmath>
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "State.h"
#include "Shader.h"
#include "Input.h"
#include "Common.h"

#include "CLightClass.h"
#include "CLavaLampSpotlight.h"
#include "CPulsatingSpotlight.h"

#include "DDSTextureLoader.h"
#include "CVector2.h" 
#include "CVector3.h" 
#include "CMatrix4x4.h"
#include "MathHelpers.h"     // Helper functions for maths
#include "GraphicsHelpers.h" // Helper functions to unclutter the code here


#include "ColourRGBA.h" 

#include <sstream>
#include <memory>
#include <vector>

#ifndef _SCENE_H_INCLUDED_
#define _SCENE_H_INCLUDED_

const float LIGHT_SPECIAL_EFFECT_UPDATE_TIMER_LIMIT = 0.1f;

struct CGraphicsShadowMap
{
	ID3D11Texture2D* Texture = nullptr; // This object represents the memory used by the texture on the GPU
	ID3D11DepthStencilView* DepthStencil = nullptr; // This object is used when we want to render to the texture above **as a depth buffer**
	ID3D11ShaderResourceView* ShaderResourceView = nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)
};

//--------------------------------------------------------------------------------------
// Scene Geometry and Layout
//--------------------------------------------------------------------------------------
// Store lights in an array in this exercise


const int CUBETEXTURECOUNT = 6;
static const std::string ARR_CUBETEXTURES[CUBETEXTURECOUNT] = { 
	"Wood2.jpg" , //Floor texture
	"Flare.jpg", //Softbody[0] texture.
	"cement_06.jpg",
	"PatternDiffuseSpecular.dds" ,
	"PatternNormal.dds",
	"PatternNormalHeight.dds" 
};
static const int SHOW_SPRINGS_LIMIT = 4;

//const int arrSphereSize = 2;
const int ARR_SOFT_BODY_COUNT = 2;
const int ARR_SCENE_COUNT = 3;
const int ARR_BOUNDARY_COUNT = 5;

const float WALL_RADIUS = 200.0f;
const CVector3 WallPositions[ARR_BOUNDARY_COUNT] =
{
	CVector3(-WALL_RADIUS,50,0),
	CVector3(WALL_RADIUS,50,0),
	CVector3(0,50,-WALL_RADIUS),
	CVector3(0,50,WALL_RADIUS),
	CVector3(0,WALL_RADIUS,0)
};

const float CUBE_SPEED = 1.125f;
const int GRAVITY_LIMIT = 500.f;

const int SPOT_LIGHT_SHADOW_MAP_COUNT = LIGHT_COUNTER;


typedef class Camera;




const eLightType arrLightAssign[LIGHT_COUNTER]
{
	DirectionalLight,
	DirectionalLight
};


class SceneManager
{
public:
	SceneManager() 
	{
		for (int i = 0; i < CUBETEXTURECOUNT; ++i)
		{
			gCubeDiffuseSpecularMap[i] = nullptr;
			gCubeDiffuseSpecularMapSRV[i] = nullptr;
		}
		//for (int i = 0; i < 6; ++i)
		//{
		//	gReflectionDiffuseCubeMap1[i] = nullptr;
		//	gReflectionDiffuseCubeMapSRV1[i] = nullptr;
		//}


		for (int i = 0; i < LIGHT_COUNTER; ++i)
		{
			SetType(arrLightAssign[i]);
		}
	}


	void RunScene(float frameTime);
	void ReleaseResources();
	bool SetupScene() {return (InitGeometry() && InitScene());}
private:

	int currScene = 0;
	int gravityStrength = 150.f;
	CVector3 Cube0momentum = { .0f,.0f,.0f };
	const float cubeDrag = 0.005f;

	void SetType(eLightType input)
	{
		switch (input)
		{
		case PulsatingLight:
			UnqPtr_Lights.push_back(std::move(std::unique_ptr<CLightClass>(new PulsatingSpotlight()))); //Light will pulse on and off
			break;
		case LavaLamp:
			UnqPtr_Lights.push_back(std::move(std::unique_ptr<CLightClass>(new CLavaLampSpotlight()))); //Light will gradually change colour
			break;
		default:			
			UnqPtr_Lights.push_back(std::move(std::unique_ptr<CLightClass>(new CLightClass()))); //Basic light with no effects
			break;
		}		
	}
	std::vector<std::unique_ptr<CLightClass>> UnqPtr_Lights;
	
	
	void UpdateScene();
	void RenderScene();
	void RenderSceneFromCamera(Camera* camera);
	void RenderDepthBufferForLightIndex(int lightIndex);

	bool InitScene();
	bool InitGeometry();

	enum SpringShowingNumbers
	{
		NodeRepShow = 2,
		SpringRepShow = 3
	};

	float frameTime;
	int   showSprings = 0; //There are multiple layers of springs. This dictates how they're shown
	int   showSpringNum = 0; //As a hidden command the user map press P to increment the shown springs, allowing the user to see the order the springs are generated.
	int   effectSpringType[3]; //Buttons for the gui to use, choosing which springs to effect when changes are applied.
	int   isCollisionOn = 0; //Toggle collisions.

	int GUIOutputCopy_SpringStrengthModel = 0;


	bool SwitchControl = false;
	int light0ColourModifier[3] = { 1,1,1 };
	int light1StrengthModifier = 1;

	CVector3 gAmbientColour = { 0.125f, 0.125f, 0.3f }; // Background level of light (slightly bluish to match the far background, which is dark blue)
	float    gSpecularPower = 256; // Specular power controls shininess - same for all models in this app

	ColourRGBA gBackgroundColor = { 0.55f, 0.55f, 0.8f, 1.0f };

	Mesh* gSoftBodyMesh[ARR_SCENE_COUNT * ARR_SOFT_BODY_COUNT];

	Mesh* gSpringMesh;
	Mesh* gFloorMesh;
	Mesh* gLightMesh;
	Mesh* gBoundaryMesh;

	Model* gSoftBody[ARR_SCENE_COUNT*ARR_SOFT_BODY_COUNT];
	Model* gGround;
	Model* gWall[ARR_BOUNDARY_COUNT];

	std::vector<Model*> VectorRep;
	std::vector<Model*> SpringRep;
	Camera* gCamera;



	CGraphicsShadowMap gSpotShadowMap[SPOT_LIGHT_SHADOW_MAP_COUNT];

	ID3D11Resource* gStaticReflectionCubeMap = nullptr;
	ID3D11ShaderResourceView* gStaticReflectionCubeMapSRV = nullptr;

	ID3D11Resource* gCubeDiffuseSpecularMap[CUBETEXTURECOUNT];
	ID3D11ShaderResourceView* gCubeDiffuseSpecularMapSRV[CUBETEXTURECOUNT];

	//ID3D11Resource* gReflectionDiffuseCubeMap1[6]; //6 faces on a cube
	//ID3D11ShaderResourceView* gReflectionDiffuseCubeMapSRV1[6];
	
};

// Prepare the geometry required for the scene
// Returns true on success


// Layout the scene
// Returns true on success



//--------------------------------------------------------------------------------------
// Scene Render and Update
//--------------------------------------------------------------------------------------


// frameTime is the time passed since the last frame


#endif //_SCENE_H_INCLUDED_

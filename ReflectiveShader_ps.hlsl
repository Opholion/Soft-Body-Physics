//--------------------------------------------------------------------------------------
// Per-Pixel Lighting Pixel Shader
//--------------------------------------------------------------------------------------
// Pixel shader receives position and normal from the vertex shader and uses them to calculate
// lighting per pixel. Also samples a samples a diffuse + specular texture map and combines with light colour.

#include "Common.hlsli" // Shaders can also use include files - note the extension


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// Here we allow the shader access to a texture that has been loaded from the C++ side and stored in GPU memory.
// Note that textures are often called maps (because texture mapping describes wrapping a texture round a mesh).
// Get used to people using the word "texture" and "map" interchangably.
TextureCube DiffuseSpecularMap : register(t0); // Textures here can contain a diffuse map (main colour) in their rgb channels and a specular map (shininess) in the a channel
//TextureCube
SamplerState TexSampler      : register(s0); // A sampler is a filter for a texture like bilinear, trilinear or anisotropic - this is the sampler used for the texture above

Texture2D ShadowMapLight1 : register(t1);
Texture2D ShadowMapLight2 : register(t2);
Texture2D ShadowMapLight3 : register(t3); // Texture holding the view of the scene from a light

SamplerState PointClamp1   : register(s1); // No filtering for shadow maps (you might think you could use trilinear or similar, but it will filter light depths not the shadows cast...)
SamplerState PointClamp2   : register(s2);
SamplerState PointClamp3   : register(s3);

//TextureCube cubeReflection : register(t0);


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Pixel shader entry point - each shader has a "main" function
// This shader just samples a diffuse texture map
float4 main(LightingPixelShaderInput input) : SV_Target
{

// Normal might have been scaled by model scaling or interpolation so renormalise
input.worldNormal = normalize(input.worldNormal);

// Direction from pixel to camera
float3 cameraDirection = normalize(gCameraPosition - input.worldPosition);


float3 textureColour = DiffuseSpecularMap.Sample(TexSampler, reflect(-cameraDirection, input.worldNormal));


float speedMod = 5.0f;
textureColour.x *= (8 * (gWiggle / speedMod - (int)gWiggle / speedMod));


	return float4(textureColour,1);// float4(finalColour, 1.0f); // Always use 1.0f for output alpha - no alpha blending in this lab
}
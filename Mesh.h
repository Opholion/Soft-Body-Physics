//--------------------------------------------------------------------------------------
// Class encapsulating a mesh
//--------------------------------------------------------------------------------------
// The mesh class splits the mesh into sub-meshes that only use one texture each.
// ** THIS VERSION WILL ONLY KEEP THE FIRST SUB-MESH OTHER PARTS WILL BE MISSING **
// ** DO NOT USE THIS VERSION FOR PROJECTS **
// The class also doesn't load textures, filters or shaders as the outer code is
// expected to select these things. A later lab will introduce a more robust loader.

//#include "common.h"
#include "SpringPoint.h"
#include "NodePoint.h"

#include <vector>
#include <string>

#ifndef _MESH_H_INCLUDED_
#define _MESH_H_INCLUDED_

constexpr bool isCoreNode = true;
constexpr int faceNum = 4;
constexpr int   SPRING_PLACEMENT_INCREMENT = 2; //Secondary value is the increment
constexpr float LOOP_LIMIT_MOD = 1.f;

constexpr float SPRING_MULT = 4.f + (.175/2.30f);

constexpr float SPRING_COEFFICIENT = 30.f * SPRING_MULT;
//Generic spring coefficient. Effects the models bindings and can be used to enforce the original shape. Has an 8/11 priority on the models nodes.
//This pushes the model to keep its size. The NodeToCentral springs cannot tell if they're pushing outwards or inwards, which can make objects scale up/down and stabilize when under great pressure

constexpr float NodeToCentralStrength =  5.f * SPRING_MULT;
//This is the connection each vector has the the core nodes. This will bind the core to the model. A good ratio will allow it to reform from being 2D and keep it structurally sound.
//Has a 3/11 priority on the models nodes.

constexpr float NODE_CENTER_STRENGTH = 0;// (NodeToCentralStrength - SPRING_COEFFICIENT) / 5.f;



constexpr float CentralNodeStrength = 22.5f * SPRING_MULT;
//This is the core strength. Without a high enough "NodeToCentral" is can cause sections to flip but with a good ratio it will force objects to keep their volume
//NodeToCentral has a priority on this though it depends on the models vertices count. 

constexpr float centralNodePosition = 3.75f; //Never make 100%


class Mesh
{
private:

    //???aiMesh STORED_assimpMesh;
    unsigned int       mVertexSize;             // Size in bytes of a single vertex (depends on what it contains, uvs, tangents etc.)
    ID3D11InputLayout* mVertexLayout = nullptr; // DirectX specification of data held in a single vertex

    // GPU-side vertex and index buffers
    unsigned int       mNumVertices;
    ID3D11Buffer* mVertexBuffer = nullptr;


    unsigned int       mNumIndices;
    ID3D11Buffer* mIndexBuffer = nullptr;

   
    //std::vector<NodeData> VertexData;




public:    
    std::vector<SpringPoint*> SpringData;
    Node VertexData;
    

    // Pass the name of the mesh file to load. Uses assimp (http://www.assimp.org/) to support many file types
    // Optionally request tangents to be calculated (for normal and parallax mapping - see later lab)
    // Will throw a std::runtime_error exception on failure (since constructors can't return errors).
    Mesh(const std::string& fileName, bool requireTangents = false, bool isCollision = false);

    ~Mesh();

    void setupSpring(std::vector<int> *input);


    // The render function assumes shaders, matrices, textures, samplers etc. have been set up already.
    // It simply draws this mesh with whatever settings the GPU is currently using.
    void Render();

    int getSpringSize()
    {
        return SpringData.size();
    }

    float getSpringStrength(int index)
    {
        return SpringData[index]->SpringCoefficient;
    }


    void SetSpringStrength(int index, float input)
    {
        SpringData[index]->SpringCoefficient = input;      
    }

    bool isParent(int input)
    {
        return (VertexData.getParent(input) == NULL);
    }

    CVector3* getSpringParent(int index, int parentID /*0-1*/)
    {
        if (parentID <= 1 && parentID >= 0 && index < SpringData.size())
        {
            CVector3* temp = &SpringData[index]->Parents[parentID]->BasicData.Position;
            return temp;
        }
      
        return nullptr;
    }

    bool isRepeatSpring(NodeData* index1, NodeData* index2)
    {
        //First we need to make sure that they are not the same and hold a value
        if (index1 == index2 ||
            (index1 == nullptr || index2 == nullptr)
            )
        {
            //If either of these conditions are true then return true as the spring would be invalid. 
            //(IsRepeatSpring returns true due to it being formatted as: "Is this a repeated spring?" and should return true when the spring is invalid.)
            return true;
        }

        //Slow but it should, at most, be limited to around 6-12 springs. Visually forming both an X and a + combined with some room for error and potentially other dimensions beyond XY.
        int loopSize[2];
            loopSize[0] = index1->ConnectedNodes.size();
            loopSize[1] = index2->ConnectedNodes.size();
        for (int i = 0; i < loopSize[0]; ++i)
        {
            //Iterate through each of index1's connected nodes and make sure they've not already been bound. 
            //This should be fine as when a new spring is formed it adds each parent to the connectedNodes list.
            if (index1->ConnectedNodes[i] == index2)
            {
                //If this is ever true then the spring is invalid.
                return true;
            }
        }

        //If no issues are found then the spring is valid.
        return false;
    }



};






#endif //_MESH_H_INCLUDED_


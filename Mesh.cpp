//--------------------------------------------------------------------------------------
// Class encapsulating a mesh
//--------------------------------------------------------------------------------------
// The mesh class splits the mesh into sub-meshes that only use one texture each.
// ** THIS VERSION WILL ONLY KEEP THE FIRST SUB-MESH OTHER PARTS WILL BE MISSING **
// ** DO NOT USE THIS VERSION FOR PROJECTS **
// The class also doesn't load textures, filters or shaders as the outer code is
// expected to select these things. A later lab will introduce a more robust loader.

#include "Mesh.h"
#include "Shader.h" // Needed for helper function CreateSignatureForVertexLayout
#include "CVector2.h" 
#include "CVector3.h" 

#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <memory>


void getCentreOfMass(CVector3 potentialInput, CVector3* currentInput, bool isGreater)
{
    if (isGreater)
    {
        if (potentialInput.x > currentInput->x)
        {
            currentInput->x = potentialInput.x;
        }
        if (potentialInput.y > currentInput->y)
        {
            currentInput->y = potentialInput.y;
        }
        if (potentialInput.z > currentInput->z)
        {
            currentInput->z = potentialInput.z;
        }
    }
    else
    {
        if (potentialInput.x < currentInput->x)
        {
            currentInput->x = potentialInput.x;
        }
        if (potentialInput.y < currentInput->y)
        {
            currentInput->y = potentialInput.y;
        }
        if (potentialInput.z < currentInput->z)
        {
            currentInput->z = potentialInput.z;
        }
    }
}

// Pass the name of the mesh file to load. Uses assimp (http://www.assimp.org/) to support many file types
// Optionally request tangents to be calculated (for normal and parallax mapping - see later lab)
// Will throw a std::runtime_error exception on failure (since constructors can't return errors).
Mesh::Mesh(const std::string& fileName, bool requireTangents /*= false*/, bool isCollision /*= false. Disable requireTangent*/)
{
    //NOTE: RequireTangents is disabled if isCollision is true.6


    Assimp::Importer importer;

    // Flags for processing the mesh. Assimp provides a huge amount of control - right click any of these
    // and "Peek Definition" to see documention above each constant
    unsigned int assimpFlags = aiProcess_MakeLeftHanded | //
                               aiProcess_GenSmoothNormals | //
                               aiProcess_FixInfacingNormals | //
                               aiProcess_GenUVCoords | //
                               aiProcess_TransformUVCoords | //
                               aiProcess_FlipUVs | //
                               aiProcess_FlipWindingOrder | //
                               aiProcess_Triangulate | //
                               aiProcess_PreTransformVertices |
                               aiProcess_ImproveCacheLocality | //
                               aiProcess_SortByPType | //
                               aiProcess_FindInvalidData |  //
                               aiProcess_OptimizeMeshes | //
                               aiProcess_FindInstances | //
                               aiProcess_FindDegenerates | //
                               aiProcess_RemoveRedundantMaterials | //
                               aiProcess_Debone | //
                               aiProcess_RemoveComponent; //
                                 //  aiProcess_JoinIdenticalVertices is Disabled to bring order to the vertices loadup.

    // Flags to specify what mesh data to ignore
    int removeComponents = aiComponent_LIGHTS | aiComponent_CAMERAS | aiComponent_TEXTURES | aiComponent_COLORS | 
                           aiComponent_BONEWEIGHTS  | aiComponent_ANIMATIONS | aiComponent_MATERIALS;

    // Add / remove tangents as required by user
    if (requireTangents)
    {
        assimpFlags |= aiProcess_CalcTangentSpace;
    }
    else
    {
        removeComponents |= aiComponent_TANGENTS_AND_BITANGENTS;
    }

    // Other miscellaneous settings
    importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f); // Smoothing angle for normals
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);  // Remove points and lines (keep triangles only)
    importer.SetPropertyBool(AI_CONFIG_PP_FD_REMOVE, true);                 // Remove degenerate triangles
    importer.SetPropertyBool(AI_CONFIG_PP_DB_ALL_OR_NONE, true);            // Default to removing bones/weights from meshes that don't need skinning
  
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removeComponents);

    // Import mesh with assimp given above requirements - log output
    Assimp::DefaultLogger::create("", Assimp::DefaultLogger::VERBOSE);
    const aiScene* scene = importer.ReadFile(fileName, assimpFlags);
    Assimp::DefaultLogger::kill();
    if (scene == nullptr)  throw std::runtime_error("Error loading mesh (" + fileName + "). " + importer.GetErrorString());
    if (scene->mNumMeshes == 0)  throw std::runtime_error("No usable geometry in mesh: " + fileName);

    //-----------------------------------
    
    // Only importing first submesh - There is no requirement for any more.
    aiMesh* assimpMesh = scene->mMeshes[0];

    std::string subMeshName = assimpMesh->mName.C_Str();


    
    //-----------------------------------

    // Check for presence of position and normal data. Tangents and UVs are optional.
    std::vector<D3D11_INPUT_ELEMENT_DESC> vertexElements;
   
    unsigned int offset = 0;

    if (!assimpMesh->HasPositions())  throw std::runtime_error("No position data for sub-mesh " + subMeshName + " in " + fileName);
    unsigned int positionOffset = offset;
    vertexElements.push_back({ "Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, positionOffset, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    offset += 12;

    if (!assimpMesh->HasNormals())  throw std::runtime_error("No normal data for sub-mesh " + subMeshName + " in " + fileName);
    unsigned int normalOffset = offset;
    vertexElements.push_back({ "Normal", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, normalOffset, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    offset += 12;

    unsigned int tangentOffset = offset;
    if (!isCollision && requireTangents)
    {
        if (!assimpMesh->HasTangentsAndBitangents())  throw std::runtime_error("No tangent data for sub-mesh " + subMeshName + " in " + fileName);
        vertexElements.push_back({ "Tangent", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, tangentOffset, D3D11_INPUT_PER_VERTEX_DATA, 0 });
        offset += 12;
    }

    unsigned int uvOffset = offset;
    if (assimpMesh->GetNumUVChannels() > 0 && assimpMesh->HasTextureCoords(0))
    {
        if (assimpMesh->mNumUVComponents[0] != 2)  throw std::runtime_error("Unsupported texture coordinates in " + subMeshName + " in " + fileName);
        vertexElements.push_back({ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, uvOffset, D3D11_INPUT_PER_VERTEX_DATA, 0 });
        offset += 8;
    }

    mVertexSize = offset;

    // Create a "vertex layout" to describe to DirectX what is data in each vertex of this mesh
    auto shaderSignature = CreateSignatureForVertexLayout(vertexElements.data(), static_cast<int>(vertexElements.size()));
    HRESULT hr = gD3DDevice->CreateInputLayout(vertexElements.data(), static_cast<UINT>(vertexElements.size()),
                                               shaderSignature->GetBufferPointer(), shaderSignature->GetBufferSize(),
                                               &mVertexLayout);
    if (shaderSignature)  shaderSignature->Release();
    if (FAILED(hr))  throw std::runtime_error("Failure creating input layout for " + fileName);



    //-----------------------------------

    // Create CPU-side buffers to hold current mesh data - exact content is flexible so can't use a structure for a vertex - so just a block of bytes
    // Note: for large arrays a unique_ptr is better than a vector because vectors default-initialise all the values which is a waste of time.
    mNumVertices = assimpMesh->mNumVertices;


    mNumIndices  = assimpMesh->mNumFaces * 4;

    auto vertices = std::make_unique<unsigned char[]>(mNumVertices * mVertexSize);
    auto indices  = std::make_unique<unsigned char[]>(mNumIndices * 4); // Using 32 bit indexes (4 bytes) for each indeex
    std::vector<BasicNode> nodeInput;
    //-----------------------------------

    // Copy mesh data from assimp to our CPU-side vertex buffer


    CVector3* assimpPosition = reinterpret_cast<CVector3*>(assimpMesh->mVertices); //This is where vertices are set.

    unsigned char* position = vertices.get() + positionOffset;
    unsigned char* positionEnd = position + mNumVertices * mVertexSize;
    while (position != positionEnd)
    {

        BasicNode positionHolder;
        positionHolder.Position = *assimpPosition;
        nodeInput.push_back(positionHolder);

        *(CVector3*)position = *assimpPosition;
        position += mVertexSize;
        ++assimpPosition;

    }

    CVector3* assimpNormal = reinterpret_cast<CVector3*>(assimpMesh->mNormals);
    unsigned char* normal = vertices.get() + normalOffset;
    unsigned char* normalEnd = normal + mNumVertices * mVertexSize;

    int i = 0; 
    while (normal != normalEnd)
    {
        if (i < nodeInput.size())
        {
            nodeInput[i].Normal = *assimpNormal;
        }
        else
        {
            BasicNode NormalHolder;
            NormalHolder.Normal = *assimpPosition;
            nodeInput.push_back(NormalHolder);
        }
        ++i;

        *(CVector3*)normal = *assimpNormal;
        normal += mVertexSize;
        ++assimpNormal;
    }

    //Currently not stored a version of tangents on the CPU-side.
    if (!isCollision && requireTangents)
    {
      CVector3* assimpTangent = reinterpret_cast<CVector3*>(assimpMesh->mTangents);
      unsigned char* tangent =  vertices.get() + tangentOffset;
      unsigned char* tangentEnd = tangent + mNumVertices * mVertexSize;
      while (tangent != tangentEnd)
      {
        *(CVector3*)tangent = *assimpTangent;
        tangent += mVertexSize;
        ++assimpTangent;
      }
    }

    if (assimpMesh->GetNumUVChannels() > 0 && assimpMesh->HasTextureCoords(0))
    {
        aiVector3D* assimpUV = assimpMesh->mTextureCoords[0];
        unsigned char* uv = vertices.get() + uvOffset;
        unsigned char* uvEnd = uv + mNumVertices * mVertexSize;

        int i = 0;
        while (uv != uvEnd)
        {
            if (i < nodeInput.size())
            {
                nodeInput[i].UV = CVector2(assimpUV->x, assimpUV->y);
            }
            else
            {
                BasicNode UVHolder;
                UVHolder.UV = CVector2(assimpUV->x, assimpUV->y);
                nodeInput.push_back(UVHolder);
            }
            ++i;

            *(CVector2*)uv = CVector2(assimpUV->x, assimpUV->y); //Make an array the size of the number of vertices for nodeData and set the uv as this, interating through
            uv += mVertexSize;
            ++assimpUV;



        }
    }


 
    std::vector<aiFace> faces;
    //-----------------------------------

    // Copy face data from assimp to our CPU-side index buffer
    if (!assimpMesh->HasFaces())  throw std::runtime_error("No face data in " + subMeshName + " in " + fileName);

    DWORD* index = reinterpret_cast<DWORD*>(indices.get());
    for (unsigned int face = 0; face < assimpMesh->mNumFaces; ++face)
    {
        
        *index++ = assimpMesh->mFaces[face].mIndices[0];

        *index++ = assimpMesh->mFaces[face].mIndices[1];

        *index++ = assimpMesh->mFaces[face].mIndices[2];

        faces.push_back(assimpMesh->mFaces[face]);
    }
    if (isCollision)
    {

        CVector3 CentreOfMass[3] = { CVector3(.0,.0,.0),CVector3(.0,.0,.0) };

        CVector3 *temp;
        aiVector3D* assimpUV = assimpMesh->mTextureCoords[0];
        for (int i = 0; i < mNumVertices; ++i, ++assimpUV)
        {

            getCentreOfMass(CVector3(assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z), &CentreOfMass[0], 0);
            getCentreOfMass(CVector3(assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z), &CentreOfMass[1], 1);
        }


        CentreOfMass[2] = (CentreOfMass[0] + CentreOfMass[1]) / 2;

        for (int i = 0; i < nodeInput.size(); ++i)
        {
            NodeData input;
            input.BasicData = nodeInput[i];
            VertexData.addNode(std::move(input), 1.0f, false);
        }


        if (isCoreNode)
        {

            float mult = 1.0f;
            for (int i = 0; i < 2; ++i)
            {
                NodeData temp;

                //

                CVector3 NewPosition = CentreOfMass[2];

                if (NewPosition.x <= 0.01f && NewPosition.x >= -0.01f)
                {
                    NewPosition.x += (CentreOfMass[1].x) * centralNodePosition;
                    NewPosition.x *= mult;
                }
                else {
                    NewPosition.x += (mult * ((CentreOfMass[2].x - CentreOfMass[0].x) * centralNodePosition));
                }
                temp.BasicData.Position = NewPosition;
                temp.BasicData.Normal = CVector3(.0f, .0f, .0f);
                temp.BasicData.UV = CVector2(0, 0);

                VertexData.addNode(std::move(temp), 1.0f, true);

                //

                NewPosition = CentreOfMass[2];

                if (NewPosition.y <= 0.01f && NewPosition.y >= -0.01f)
                {
                    NewPosition.y += (CentreOfMass[1].y) * centralNodePosition;
                    NewPosition.y *= mult;
                }
                else {
                    NewPosition.y += ((mult * ((CentreOfMass[2].y - CentreOfMass[0].y) * centralNodePosition)));
                }


                temp.BasicData.Position = NewPosition;
                temp.BasicData.Normal = CVector3(.0f, .0f, .0f);
                temp.BasicData.UV = CVector2(0, 0);

                VertexData.addNode(std::move(temp), 1.0f, true);

                //

                NewPosition = CentreOfMass[2];


                if (NewPosition.z <= 0.01f && NewPosition.z >= -0.01f)
                {
                    NewPosition.z += (CentreOfMass[1].z) * centralNodePosition;
                    NewPosition.z *= mult;
                }
                else {
                    NewPosition.z += ((mult * ((CentreOfMass[2].z - CentreOfMass[0].z) * centralNodePosition)));
                }
                temp.BasicData.Position = NewPosition;
                temp.BasicData.Normal = CVector3(.0f, .0f, .0f);
                temp.BasicData.UV = CVector2(0, 0);


                VertexData.addNode(std::move(temp), 1.0f, true);

                //



                mult *= -1.0f;
            }
        }
    
        //Will need the vertices order for creating faces and binding springs.
        std::vector<int> input;
        int inputLoopLimit = assimpMesh->mNumFaces ;

        for (size_t i = 0; i < inputLoopLimit; ++i)
        {
            //Ideally a face has only 3 vertices. This will cause an error to occur, otherwise as most of the code uses that assumption.
            if (assimpMesh->mFaces->mNumIndices == 3)
            {
                input.push_back(assimpMesh->mFaces[i].mIndices[0]);
                input.push_back(assimpMesh->mFaces[i].mIndices[1]);
                input.push_back(assimpMesh->mFaces[i].mIndices[2]);
            }
        }

      //  for(int i = 0; i < faces[0].)
       // input.push_back(assimpMesh->mFaces[].mIndices[0]);

        VertexData.setupRootSize();
        setupSpring(&input);

        
    }
    //-----------------------------------

    D3D11_BUFFER_DESC bufferDesc;
    D3D11_SUBRESOURCE_DATA initData;


    // Create GPU-side index buffer and copy the vertices imported by assimp into it
    bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER; // Indicate it is an index buffer
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;         // Default usage for this buffer - we'll see other usages later
    bufferDesc.ByteWidth = mNumIndices * sizeof(DWORD); // Size of the buffer in bytes
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    initData.pSysMem = indices.get(); // Fill the new index buffer with data loaded by assimp
    
    hr = gD3DDevice->CreateBuffer(&bufferDesc, &initData, &mIndexBuffer);
    if (FAILED(hr))  throw std::runtime_error("Failure creating index buffer for " + fileName);


    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // Indicate it is a vertex buffer
    bufferDesc.ByteWidth = mNumVertices * mVertexSize; // Size of the buffer in bytes
        //If the model is something you can collide with then you (for this) will need to update it real-time
    if (isCollision)
    {
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        initData.pSysMem = &nodeInput.at(0);
    }
    else
    {
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.CPUAccessFlags = 0;
        initData.pSysMem = vertices.get();// Fill the new vertex buffer with data loaded by assimp
    }
    bufferDesc.MiscFlags = 0;
    

    hr = gD3DDevice->CreateBuffer(&bufferDesc, &initData, &mVertexBuffer);
    if (FAILED(hr))  throw std::runtime_error("Failure creating vertex buffer for " + fileName);
}


Mesh::~Mesh()
{
    if (mIndexBuffer)   mIndexBuffer ->Release();
    if (mVertexBuffer)  mVertexBuffer->Release();
    if (mVertexLayout)  mVertexLayout->Release();
}

int ifSpringWithinBounds(float y1, float y2, float y3, float yNew)
{
    while (y1 < y2 || y2 < y3)
    {
        if (y3 > y2)
        {
            float temp = y2;
            y2 = y3;
            y3 = temp;
        }
        if (y2 > y1)
        {
            float temp = y1;
            y1 = y2;
            y2 = temp;
        }
    }

    if (yNew <= y1 && yNew >= y3)
    {
        return 1;
    }
    return 0;
}
//Note:
//Need to figure out how to figure out the opposite point
//Need to make sure they're not placed in the same place twice


void Mesh::setupSpring(std::vector<int>* input)
{
    int loopLimit;
    int CurrentLayerFaceCount = 0;
    //loopLimit = VertexData.getSize();
    loopLimit = input->size();


    for (int face = 0; face < loopLimit * LOOP_LIMIT_MOD; face += 6)
    {
        //This is the initial polygon face
        NodeData* a = VertexData.getNode(input->at(face + 0));
        NodeData* b = VertexData.getNode(input->at(face + 1));
        NodeData* c = VertexData.getNode(input->at(face + 2));

       if (!isRepeatSpring(a, b))
       {
           SpringData.push_back(new SpringPoint(*a,*b, SPRING_COEFFICIENT));
       }
       if (!isRepeatSpring(b,c))
       {
           SpringData.push_back(new SpringPoint(*b,*c, SPRING_COEFFICIENT));
       }
       if (!isRepeatSpring(c,a))
       {
           SpringData.push_back(new SpringPoint(*c,*a, SPRING_COEFFICIENT));
       }


       //To create a whole face you must check which ones have no yet been assigned.
       //The only face after the 3rd one that is not any of the previously used one 
       //will be the final, untouched, corner. This will only work without the
       //"aiProcess_JoinIdenticalVertices" flag as it removes any form of order
       //With the polygon order.
       NodeData* d;

       for (int i = 0; i < 3; ++i)
       {
           d = VertexData.getNode(input->at(face + (i + 3)));
           if (d != a && d != b && d != c)
           {
               i = 3;
           }
       }

       if (!isRepeatSpring(d, a))
       {
           SpringData.push_back(new SpringPoint(*d, *a, SPRING_COEFFICIENT));
       }
       if (!isRepeatSpring(d, b))
       {
           SpringData.push_back(new SpringPoint(*d, *b, SPRING_COEFFICIENT));
       }
       if (!isRepeatSpring(d, c))
       {
           SpringData.push_back(new SpringPoint(*d, *c, SPRING_COEFFICIENT));
       }

       VertexData.addFace(a, b, c);
       VertexData.addFace(d, b, c);
      // SpringData.push_back(new SpringPoint(*VertexData.getNode(input->at(face)), *VertexData.getNode(input->at(loopLimit-1)), SPRING_COEFFICIENT));
    }

    int VertexSize = VertexData.getSize();


    for (int i = 0; i < 0; i += SPRING_PLACEMENT_INCREMENT)
    {
        SpringData.push_back(new SpringPoint(*VertexData.getNode(input->at(i)), *VertexData.getNode(VertexSize - 1), NODE_CENTER_STRENGTH));

    }

    constexpr int CoreNodeArrayPosition = 6;
    if (isCoreNode)
    {
        NodeData* a;
        NodeData* b;

        for (int i = VertexSize - CoreNodeArrayPosition; i < VertexSize; ++i)
        {
            a = VertexData.getNode(i);

            for (int j = VertexSize - CoreNodeArrayPosition; j < VertexSize; ++j)
            {
                b = VertexData.getNode(j);

                if (!isRepeatSpring(a, b))
                {
                    SpringData.push_back(new SpringPoint(*a, *b, CentralNodeStrength));
                }
            }
        }
        for (int i = 0; i < VertexSize; ++i)
        {
            a = VertexData.getNode(i);
            for (int j = VertexSize - CoreNodeArrayPosition; j < VertexSize; ++j)
            {
                b = VertexData.getNode(j);

                if (!isRepeatSpring(a, b))
                {
                    SpringData.push_back(new SpringPoint(*a, *b, CentralNodeStrength));
                }
            }
        }
    }

}



// The render function assumes shaders, matrices, textures, samplers etc. have been set up already.
// It simply draws this mesh with whatever settings the GPU is currently using.
void Mesh::Render()
{
    // Set vertex buffer as next data source for GPU
    UINT stride = mVertexSize;
    UINT offset = 0;

    //No positions should be at 0,0,0 so this should be fine. Can be refined later.
    if (VertexData.getSize() > 0 && 
        VertexData.getPosition(0).x >=-0.01f && 
        VertexData.getPosition(0).x <= 0.01f && 
        VertexData.getPosition(0).y >=-0.01f && 
        VertexData.getPosition(0).y <= 0.01f && 
        VertexData.getPosition(0).z >=-0.01f && 
        VertexData.getPosition(0).z <= 0.01f)
    {}
    else if(VertexData.getSize() > 0)
    {        
        int loopLimit = VertexData.getSize();
        D3D11_MAPPED_SUBRESOURCE cb;
       
        //Gain access to the GPU, slowing it, and then copy over a copy of the data used to the buffer.
        gD3DContext->Map(mVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &cb);
        BasicNode* bufferData = (BasicNode*)cb.pData;

        for (int i = 0; i < loopLimit; ++i)
        {
            *bufferData++ = VertexData.GetChildNode(i)->BasicData;//*VertexData[i].BasicData;
        }


       // memcpy(cb.pData, vertices.get(), mNumVertices * mVertexSize); //CVector3 = 3 floats.  // 1 float = 4 bits of memory. (Should equal 12 memory) (50 * 12 = 600)
        gD3DContext->Unmap(mVertexBuffer, 0);
        

    }

    gD3DContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
   
    
    //mVertexLayout->SetPrivateData();
    // Indicate the layout of vertex buffer
    gD3DContext->IASetInputLayout(mVertexLayout);

    // Set index buffer as next data source for GPU, indicate it uses 32-bit integers
    gD3DContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // Using triangle lists only in this class
    gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Render mesh
    gD3DContext->DrawIndexed(mNumIndices, 0, 0);
}



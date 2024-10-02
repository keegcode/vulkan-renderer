#include "mesh.hpp"
#include "buffer.hpp"

#include <assimp/Importer.hpp>      
#include <assimp/scene.h>           
#include <assimp/postprocess.h>  
#include <cstdint>

Mesh::Mesh(const VmaAllocator& allocator, const std::string_view path) {
  Assimp::Importer importer{};

  const aiScene* scene = importer.ReadFile(
    path.data(), 
    aiProcess_Triangulate | 
    aiProcess_FlipUVs |
    aiProcess_GenNormals |
    aiProcess_GenUVCoords
  );

  if (!scene) {
    throw std::runtime_error{std::string{"Failed to read mesh file: "} + importer.GetErrorString()};
  }

  std::vector<Vertex> vertices;
  std::vector<uint16_t> indices;

  for (size_t i = 0; i < scene->mNumMeshes; i++) {
    aiMesh* assimpMesh = scene->mMeshes[i];

    for (size_t j = 0; j < assimpMesh->mNumVertices; j++) {
      Vertex vertex{};

      vertex.pos[0] = assimpMesh->mVertices[j].x;
      vertex.pos[1] = assimpMesh->mVertices[j].y;
      vertex.pos[2] = assimpMesh->mVertices[j].z;

      vertex.normals[0] = assimpMesh->mNormals[j].x;
      vertex.normals[1] = assimpMesh->mNormals[j].y;
      vertex.normals[2] = assimpMesh->mNormals[j].z;

      vertex.clr[0] = 0.4f;
      vertex.clr[1] = 0.4f;
      vertex.clr[2] = 0.4f;

      if (assimpMesh->HasTextureCoords(0)) {
        vertex.uv[0] = assimpMesh->mTextureCoords[0][j].x;
        vertex.uv[1] = assimpMesh->mTextureCoords[0][j].y;
      }

      vertices.push_back(vertex);
    }

    for (size_t j = 0; j < assimpMesh->mNumFaces; j++) {
      assert(assimpMesh->mFaces[j].mNumIndices == 3);
      indices.push_back(assimpMesh->mFaces[j].mIndices[0]);
      indices.push_back(assimpMesh->mFaces[j].mIndices[1]);
      indices.push_back(assimpMesh->mFaces[j].mIndices[2]);
    }
  }

  importer.FreeScene();
  
  vertexBuffer = Buffer{allocator, vertices.data(), static_cast<uint32_t>(sizeof(Vertex) * vertices.size()), vk::BufferUsageFlagBits::eVertexBuffer};
  indexBuffer = Buffer{allocator, indices.data(), static_cast<uint32_t>(sizeof(uint16_t) * indices.size()), vk::BufferUsageFlagBits::eIndexBuffer};

  indicesCount = indices.size(); 
}

void Mesh::destroy(const VmaAllocator& allocator) {
  vertexBuffer.destroy(allocator);
  indexBuffer.destroy(allocator);
}

#ifndef SNOWFLAKE_EIGEN_HPP
#define SNOWFLAKE_EIGEN_HPP

#include "pch.hpp"

// geometry
#include "geometry/Vertex.hpp"

namespace Eigen
{

  /**
  * @brief Calculates the 3x3 Covariance Matrix from a set of points.
  * @param points Points to calculate covariance.
  * @param pointCount Number of points in points array.
  * @return 3x3 Covariance Matrix.
  */
  INLINE glm::mat3x3 CalcCovarianceMatrix3x3(const glm::vec3* points, u32 pointCount)
  {
    glm::mat3x3 covarianceMatrix = glm::identity<glm::mat3x3>();

    for (i32 row = 0; row < 3; ++row)
    {
      for (i32 col = 0; col < 3; ++col)
      {
        f32& value = covarianceMatrix[row][col];

        for (i32 i = 0; i < (i32)pointCount; ++i)
        {
          value += points[i][row] * points[i][col];
        }

        value /= (f32)(pointCount);
      }
    }

    return covarianceMatrix;
  }

  /**
  * @brief Calculates the 3x3 Covariance Matrix from a set of vertices.
  * @param vertexArray Vertices to calculate covariance.
  * @param vertexCount Number of vertices in vertex array.
  * @return 3x3 Covariance Matrix.
  */
  INLINE glm::mat3x3 CalcCovarianceMatrix3x3(const Vertex* vertexArray, u32 vertexCount)
  {
    glm::mat3x3 covarianceMatrix = glm::identity<glm::mat3x3>();

    for (i32 row = 0; row < 3; ++row)
    {
      for (i32 col = 0; col < 3; ++col)
      {
        f32& value = covarianceMatrix[row][col];

        for (i32 i = 0; i < (i32)vertexCount; ++i)
        {
          value += vertexArray[i].position[row] * vertexArray[i].position[col];
        }

        value /= (f32)(vertexCount);
      }
    }

    return covarianceMatrix;
  }

  /**
  * Perform Eigen Decomposition on a 3x3 matrix. This solves the Characteristic
  * Polynomial Equation - doesn't require any iteration meaning this is fast.
  * @param inMatrix Matrix to perform Eigen Decomposition.
  * @param out_eigenvalues Out variable to the 3 computed eigen values. Must not be nullptr.
  * @param out_eigenvectors Out variable to the 3 computer eigen vectors. Must not be nullptr.
  */
  void EigenDecomposition3x3(const glm::mat3x3&          inMatrix,
                             FArray<f32,       3>* const out_eigenvalues,
                             FArray<glm::vec3, 3>* const out_eigenvectors);

  /**
  * @brief Computes the Eigenvalues and Eigenvectors of the inputted
  * 3x3 matrix, using the Jacobi iterative method.
  * @param inMatrix The matrix to find Eigenvalues and vectors.
  * @return The matrix will hold the 3 Eigenvectors with the
  * diagonal elements being the corresponding Eigenvalues.
  */
  glm::mat3x3 EigenDecompositionJacobi3x3(glm::mat3x3 inMatrix);

} // namespace Math

#endif //SNOWFLAKE_EIGEN_HPP

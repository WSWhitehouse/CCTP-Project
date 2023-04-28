#include "geometry/Eigen.hpp"

// core
#include "core/Logging.hpp"
#include "core/Assert.hpp"

// math
#include "math/Math.hpp"

void Eigen::EigenDecomposition3x3(const glm::mat3x3&          inMatrix,
                                  FArray<f32,       3>* const out_eigenvalues,
                                  FArray<glm::vec3, 3>* const out_eigenvectors)
{
  ASSERT_MSG(out_eigenvalues  != nullptr, "Out parameter (out_eigenvalues) is nullptr! This must be a valid memory address...");
  ASSERT_MSG(out_eigenvectors != nullptr, "Out parameter (out_eigenvectors) is nullptr! This must be a valid memory address...");
  
  // NOTE(WSWhitehouse): Zero out the output parameters to ensure they dont contain any old data
  mem_zero(out_eigenvalues, sizeof(FArray<f32, 3>));
  mem_zero(out_eigenvectors, sizeof(FArray<glm::vec3, 3>));

  // NOTE(WSWhitehouse): During this calculation the variable names follow the mathematical
  // notation as close as possible to make it easier to follow and link with the equation below.

  // Characteristic Polynomial Equation:
  //   f(λ) = det(λI − inMatrix) = λ^3 − tr(inMatrix)λ^2 + ((tr^2(inMatrix) − tr(inMatrix^2)) / 2)λ − det(inMatrix)
  //
  //   inMatrix     = matrix
  //   I     = identity matrix
  //   tr()  = trace of matrix
  //   det() = determinant of matrix
  //   λ     = eigenvalue

  constexpr const glm::mat3 I = glm::identity<glm::mat3>();

  // tr(inMatrix)
  const f32 trA = Math::MatrixTrace(inMatrix);

  // q = tr(inMatrix)/3
  // p = sqrt(tr((inMatrix − qI)^2) / 6)
  // B = (inMatrix − qI)/p

  const f32 q = trA / 3.0f;

  const glm::mat3 temp    = inMatrix - (q * I);
  const glm::mat3 tempSqr = temp * temp;
  const f32 tempSqrTr     = Math::MatrixTrace(tempSqr);

  const f32 p = glm::sqrt(tempSqrTr / 6.0f);

  // If inMatrix is a scalar multiple of the identity, then p == 0.0f
  if (glm::abs(p) <= FLT_EPSILON)
  {
    // Values
    (*out_eigenvalues)[0] = inMatrix[0][0];
    (*out_eigenvalues)[1] = inMatrix[1][1];
    (*out_eigenvalues)[2] = inMatrix[2][2];

    // Vectors
    (*out_eigenvectors)[0] = { 1.0f, 0.0f, 0.0f };
    (*out_eigenvectors)[1] = { 0.0f, 1.0f, 0.0f };
    (*out_eigenvectors)[2] = { 0.0f, 0.0f, 1.0f };
    return;
  }

  const glm::mat3 B = temp / p;

  const f32 detB     = Math::MatrixDeterminant(B);
  const f32 detBHalf = CLAMP((detB * 0.5f), -1.0f, 1.0f);

  constexpr const f32 twoThirdsPI = 2.0f * PI / 3.0f;

  const f32 angle = glm::acos(detBHalf) / 3.0f;

  // The eigenvalues of B are ordered as beta0 <= beta1 <= beta2
  const f32 beta2 = glm::cos(angle) * 2.0f;
  const f32 beta0 = glm::cos(angle + twoThirdsPI) * 2.0f;
  const f32 beta1 = -(beta0 + beta2);

  (*out_eigenvalues)[0] = q + p * beta0;
  (*out_eigenvalues)[1] = q + p * beta1;
  (*out_eigenvalues)[2] = q + p * beta2;

  // Eigenvector 0
  {
    const glm::vec3 row0 = { inMatrix[0][0] - (*out_eigenvalues)[0], inMatrix[0][1],                         inMatrix[0][2]                         };
    const glm::vec3 row1 = { inMatrix[0][1],                         inMatrix[1][1] - (*out_eigenvalues)[0], inMatrix[1][2]                         };
    const glm::vec3 row2 = { inMatrix[0][2],                         inMatrix[1][2],                         inMatrix[2][2] - (*out_eigenvalues)[0] };

    const glm::vec3 r0xr1 = glm::cross(row0, row1);
    const glm::vec3 r0xr2 = glm::cross(row0, row2);
    const glm::vec3 r1xr2 = glm::cross(row1, row2);

    const f32 d0 = glm::dot(r0xr1, r0xr1);
    const f32 d1 = glm::dot(r0xr2, r0xr2);
    const f32 d2 = glm::dot(r1xr2, r1xr2);

    f32 dMax = d0;
    u32 iMax = 0;
    if (d1 > dMax) { dMax = d1; iMax = 1; }
    if (d2 > dMax) { iMax = 2; }

    if (iMax == 0)
    {
      (*out_eigenvectors)[0] = r0xr1 / glm::sqrt(d0);
    }
    else if (iMax == 1)
    {
      (*out_eigenvectors)[0] = r0xr2 / glm::sqrt(d1);
    }
    else
    {
      (*out_eigenvectors)[0] = r1xr2 / glm::sqrt(d2);
    }
  }

  // Eigenvector 1
  {
    glm::vec3 U;
    if (glm::abs((*out_eigenvectors)[0].x) > glm::abs((*out_eigenvectors)[0].y))
    {
      const f32 invLength = 1.0f / glm::sqrt((*out_eigenvectors)[0].x * (*out_eigenvectors)[0].x + (*out_eigenvectors)[0].z * (*out_eigenvectors)[0].z);
      U = { -(*out_eigenvectors)[0].z * invLength, 0.0f, (*out_eigenvectors)[0].x * invLength };
    }
    else
    {
      const f32 invLength = 1.0f / glm::sqrt((*out_eigenvectors)[0].y * (*out_eigenvectors)[0].y + (*out_eigenvectors)[0].z * (*out_eigenvectors)[0].z);
      U = { 0.0f, (*out_eigenvectors)[0].z * invLength, -(*out_eigenvectors)[0].y * invLength };
    }

    const glm::vec3 V = glm::cross((*out_eigenvectors)[0], U);

    const glm::vec3 AU = inMatrix * U;
    const glm::vec3 AV = inMatrix * V;

    f32 m00 = glm::dot(U, AU) - (*out_eigenvalues)[1];
    f32 m01 = glm::dot(U, AV);
    f32 m11 = glm::dot(V, AV) - (*out_eigenvalues)[1];

    const f32 m00Abs = glm::abs(m00);
    const f32 m01Abs = glm::abs(m01);
    const f32 m11Abs = glm::abs(m11);

    if (m00Abs >= m11Abs)
    {

      if (MAX(m00Abs, m01Abs) > 0)
      {

        if (m00Abs >= m01Abs)
        {
          m01 /= m00;
          m00  = 1.0f / glm::sqrt(1.0f + m01 * m01);
          m01 *= m00;
        }
        else
        {
          m00 /= m01;
          m01  = 1.0f / glm::sqrt(1.0f + m00 * m00);
          m00 *= m01;
        }

        (*out_eigenvectors)[1] = m01 * U - m00 * V;

      }
      else
      {
        (*out_eigenvectors)[1] = U;
      }

    }
    else
    {
      if (MAX(m11Abs, m01Abs) > 0)
      {
        if (m11Abs >= m01Abs)
        {
          m01 /= m11;
          m11  = 1.0f / glm::sqrt(1.0f + m01 * m01);
          m01 *= m11;
        }
        else
        {
          m11 /= m01;
          m01  = 1.0f / glm::sqrt(1.0f + m11 * m11);
          m11 *= m01;
        }

        (*out_eigenvectors)[1] = m11 * U - m01 * V;
      }
      else
      {
        (*out_eigenvectors)[1] = U;
      }
    }
  }

  // Eigenvector 2
  (*out_eigenvectors)[2] = glm::cross((*out_eigenvectors)[0], (*out_eigenvectors)[1]);
}

glm::mat3x3 Eigen::EigenDecompositionJacobi3x3(glm::mat3x3 inMatrix)
{
  return glm::identity<glm::mat3x3>();
}

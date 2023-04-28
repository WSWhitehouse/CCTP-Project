#ifndef SNOWFLAKE_BOUNDING_BOX_HPP
#define SNOWFLAKE_BOUNDING_BOX_HPP

#include "pch.hpp"

/**
* @brief A 3D axis aligned bounding box (AABB).
*/
struct BoundingBox3D
{
  // --- MEMBER DATA --- //
  glm::vec3 minimum;
  glm::vec3 maximum;

  /** @brief Get the center of this bounding box. */
  [[nodiscard]] INLINE glm::vec3 GetCenter() const { return (minimum + maximum) * 0.5f; }

  /** @brief Get the total Size of this bounding box. */
  [[nodiscard]] INLINE glm::vec3 GetSize() const { return maximum - minimum; }

  [[nodiscard]] INLINE glm::vec3 GetExtents() const { return GetSize() * 0.5f; }

  /**
  * @brief Grows the bounding box to encapsulate the point.
  * @param point Point to encapsulate.
  */
  INLINE void EncapsulatePoint(const glm::vec3& point)
  {
    f32 minX = MIN(minimum.x, point.x);
    f32 minY = MIN(minimum.y, point.y);
    f32 minZ = MIN(minimum.z, point.z);

    f32 maxX = MAX(maximum.x, point.x);
    f32 maxY = MAX(maximum.y, point.y);
    f32 maxZ = MAX(maximum.z, point.z);

    minimum = glm::vec3(minX, minY, minZ);
    maximum = glm::vec3(maxX, maxY, maxZ);
  }

  /**
  * @brief Check if the requested point is inside this bounding box.
  * @param point Point to check.
  * @return True when inside; false otherwise.
  */
  [[nodiscard]] INLINE b8 ContainsPoint(const glm::vec3& point) const
  {
    return
    (
      point.x >= minimum.x && point.x <= maximum.x &&
      point.y >= minimum.y && point.y <= maximum.y &&
      point.z >= minimum.z && point.z <= maximum.z
    );
  }

  /**
  * @brief Check if the requested bounding box overlaps this bounding box.
  * @param aabb Bounding box to check.
  * @return True when overlapping; false otherwise.
  */
  [[nodiscard]] INLINE b8 OverlapAABB(const BoundingBox3D& aabb) const
  {
    return
    (
       minimum.x <= aabb.maximum.x && maximum.x >= aabb.minimum.x &&
       minimum.y <= aabb.maximum.y && maximum.y >= aabb.minimum.y &&
       minimum.z <= aabb.maximum.z && maximum.z >= aabb.minimum.z
    );
  }

  /**
  * @brief Combines two bounding boxes together.
  * @param lhs First bounding box.
  * @param rhs Second bounding box.
  * @return New combined bounding box.
  */
  [[nodiscard]] static INLINE BoundingBox3D Combine(const BoundingBox3D& lhs, const BoundingBox3D& rhs)
  {
    BoundingBox3D boundingBox = lhs;
    boundingBox.EncapsulatePoint(rhs.minimum);
    boundingBox.EncapsulatePoint(rhs.maximum);

    return boundingBox;
  }
};

#endif //SNOWFLAKE_BOUNDING_BOX_HPP

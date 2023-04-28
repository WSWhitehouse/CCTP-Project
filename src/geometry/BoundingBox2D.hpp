#ifndef SNOWFLAKE_BOUNDING_BOX_2D_HPP
#define SNOWFLAKE_BOUNDING_BOX_2D_HPP

#include "pch.hpp"

/**
* @brief A 2D axis aligned bounding box (AABB).
*/
struct BoundingBox2D
{
  // --- MEMBER DATA --- //
  glm::vec2 minimum;
  glm::vec2 maximum;

  /** @brief Get the center of this bounding box. */
  [[nodiscard]] INLINE glm::vec2 GetCenter() const { return (minimum + maximum) * 0.5f; }

  /** @brief Get the total Size of this bounding box. */
  [[nodiscard]] INLINE glm::vec2 GetSize() const { return maximum - minimum; }

  /**
  * @brief Grows the bounding box to encapsulate the point.
  * @param point Point to encapsulate.
  */
  INLINE void EncapsulatePoint(const glm::vec2& point)
  {
    f32 minX = MIN(minimum.x, point.x);
    f32 minY = MIN(minimum.y, point.y);

    f32 maxX = MAX(maximum.x, point.x);
    f32 maxY = MAX(maximum.y, point.y);

    minimum = glm::vec2(minX, minY);
    maximum = glm::vec2(maxX, maxY);
  }

  /**
  * @brief Check if the requested point is inside this bounding box.
  * @param point Point to check.
  * @param scale Scale factor to modify min and max by.
  * @return True when inside; false otherwise.
  */
  [[nodiscard]] INLINE b8 Contains(const glm::vec2& point, const glm::vec2& scale = {1.0f, 1.0f}) const
  {
    const glm::vec2 min = minimum * scale;
    const glm::vec2 max = maximum * scale;

    return
      (
        point.x >= min.x && point.x <= max.x &&
        point.y >= min.y && point.y <= max.y
      );
  }

  /**
  * @brief Check if the requested bounding box overlaps this bounding box.
  * @param aabb Bounding box to check.
  * @return True when overlapping; false otherwise.
  */
  [[nodiscard]] INLINE b8 Contains(const BoundingBox2D& aabb) const
  {
    return
      (
        minimum.x <= aabb.maximum.x && maximum.x >= aabb.minimum.x &&
        minimum.y <= aabb.maximum.y && maximum.y >= aabb.minimum.y
      );
  }

  /**
  * @brief Combines two bounding boxes together.
  * @param lhs First bounding box.
  * @param rhs Second bounding box.
  * @return New combined bounding box.
  */
  [[nodiscard]] static INLINE BoundingBox2D Combine(const BoundingBox2D& lhs, const BoundingBox2D& rhs)
  {
    BoundingBox2D boundingBox = lhs;
    boundingBox.EncapsulatePoint(rhs.minimum);
    boundingBox.EncapsulatePoint(rhs.maximum);

    return boundingBox;
  }
};

#endif //SNOWFLAKE_BOUNDING_BOX_2D_HPP

/******************************************************************************/
/*!
\file		Vector2D.cpp
\author 	Joshua Sim Yue Chen, joshuayuechen.sim, 2301283
\par    	email: joshuayuechen.sim@digipen.edu
\date   	March 25, 2024
\brief		This file implements the operations for 2D vector manipulation,
			including addition, subtraction, scalar multiplication, division,
			normalization, and calculating length, distance, dot product, and
			cross product magnitude.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include "pch.h"
#include "Vector2D.h"
#include <cmath>


namespace UE_Vector
{
	/**************************************************************************/
   /*!
	   \brief Constructs a 2D vector with initial x and y components.
	   \param _x The x component.
	   \param _y The y component.
   */
   /**************************************************************************/
	Vector2D::Vector2D(float _x, float _y) : x(_x), y(_y)
	{
	}

	/**************************************************************************/
  /*!
	  \brief Adds another vector to this vector.
	  \param rhs The right-hand side vector to add.
	  \return The resultant vector after addition.
  */
  /**************************************************************************/
	Vector2D& Vector2D::operator+=(const Vector2D& rhs)
	{
		x += rhs.x;
		y += rhs.y;

		return *this;
	}

	/**************************************************************************/
   /*!
	   \brief Subtracts another vector from this vector.
	   \param rhs The right-hand side vector to subtract.
	   \return The resultant vector after subtraction.
   */
   /**************************************************************************/
	Vector2D& Vector2D::operator-=(const Vector2D& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;

		return *this;
	}

	/**************************************************************************/
   /*!
	   \brief Multiplies this vector by a scalar.
	   \param rhs The scalar to multiply by.
	   \return The resultant vector after scalar multiplication.
   */
   /**************************************************************************/
	Vector2D& Vector2D::operator*=(float rhs)
	{
		x = x * rhs;
		y = y * rhs;
		return *this;
	}

	/**************************************************************************/
/*!
	\brief Divides this vector by a scalar.
	\param rhs The scalar to divide by.
	\return The resultant vector after scalar division.
*/
/**************************************************************************/
	Vector2D& Vector2D::operator/=(float rhs)
	{

		if (rhs != 0.0f)
		{ // Check to prevent division by zero
			x /= rhs;
			y /= rhs;
		}

		return *this;
	}

	/**************************************************************************/
	   /*!
		   \brief Negates this vector.
		   \return The negated vector.
	   */
	   /**************************************************************************/
	Vector2D Vector2D::operator-() const
	{

		return Vector2D(-x, -y);
	}

	// Non-member functions
	//-------------------------------------------------------------------------

	/******************************************************************************/
/*!
	\brief Adds two vectors.
	\param lhs The left-hand side vector.
	\param rhs The right-hand side vector.
	\return The result of vector addition.
*/
/******************************************************************************/
	Vector2D operator+(const Vector2D& lhs, const Vector2D& rhs)
	{

		Vector2D output(lhs.x + rhs.x, lhs.y + rhs.y);
		return output;
	}

	/******************************************************************************/
	/*!
		\brief Subtracts one vector from another.
		\param lhs The left-hand side vector.
		\param rhs The right-hand side vector to subtract from lhs.
		\return The result of vector subtraction.
	*/
	/******************************************************************************/
	Vector2D operator-(const Vector2D& lhs, const Vector2D& rhs)
	{
		Vector2D output(lhs.x - rhs.x, lhs.y - rhs.y);
		return output;
	}

	/******************************************************************************/
	/*!
		\brief Multiplies a vector by a scalar.
		\param lhs The vector to be scaled.
		\param rhs The scalar value.
		\return The result of scalar multiplication on the vector.
	*/
	/******************************************************************************/
	Vector2D operator*(const Vector2D& lhs, float rhs)
	{
		Vector2D output(lhs.x * rhs, lhs.y * rhs);
		return output;
	}

	/******************************************************************************/
	/*!
		\brief Multiplies a scalar by a vector.
		\param lhs The scalar value.
		\param rhs The vector to be scaled.
		\return The result of scalar multiplication on the vector.
		\note This function provides commutativity for scalar-vector multiplication.
	*/
	/******************************************************************************/
	Vector2D operator*(float lhs, const Vector2D& rhs)
	{
		Vector2D output(lhs * rhs.x, lhs * rhs.y);
		return output;
	}

	/******************************************************************************/
/*!
	\brief Divides a vector by a scalar.
	\param lhs The vector to be divided.
	\param rhs The scalar divisor.
	\return The result of dividing the vector by the scalar.
	\note This operation scales down each component of the vector by the divisor.
*/
/******************************************************************************/
	Vector2D operator/(const Vector2D& lhs, float rhs)
	{
		Vector2D output(lhs.x / rhs, lhs.y / rhs);
		return output;
	}

	/**************************************************************************/
  /*!
	  \brief Normalizes the provided vector.
	  \param pResult The resultant normalized vector.
	  \param pVec0 The vector to normalize.
  */
  /**************************************************************************/
	void Vector2DNormalize(Vector2D& pResult, const Vector2D& pVec0)
	{
		// First, calculate the magnitude of pVec0
		float magnitude = sqrt(pVec0.x * pVec0.x + pVec0.y * pVec0.y);

		// Check to ensure the magnitude is not zero to avoid division by zero
		if (magnitude > 0.0f)
		{
			// Normalize each component by dividing by the magnitude
			pResult.x = pVec0.x / magnitude;
			pResult.y = pVec0.y / magnitude;
		}
		else
		{
			// If the magnitude is zero (the vector is a zero vector),
			// we cannot normalize it. A common approach is to return a zero vector.
			pResult.x = 0.0f;
			pResult.y = 0.0f;
		}
	}

	/**************************************************************************/
	  /*!
		  \brief Calculates the length of the vector.
		  \param pVec0 The vector whose length is to be calculated.
		  \return The length of the vector.
	  */
	  /**************************************************************************/
	float Vector2DLength(const Vector2D& pVec0)
	{
		// Calculate the square of the length using the components of the vector
		float lengthSquared = pVec0.x * pVec0.x + pVec0.y * pVec0.y;

		// Take the square root of the length squared to get the length
		float length = sqrt(lengthSquared);

		return length;
	}

	/**************************************************************************/
		/*!
			\brief Calculates the squared length of the vector.
			\param pVec0 The vector whose squared length is to be calculated.
			\return The squared length of the vector.
		*/
		/**************************************************************************/
	float Vector2DSquareLength(const Vector2D& pVec0)
	{
		// Calculate the square of the length using the components of the vector
		return (pVec0.x * pVec0.x + pVec0.y * pVec0.y);


	}

	/**************************************************************************/
		/*!
			\brief Calculates the distance between two vectors.
			\param pVec0 The first vector.
			\param pVec1 The second vector.
			\return The distance between the two vectors.
		*/
		/**************************************************************************/
	float Vector2DDistance(const Vector2D& pVec0, const Vector2D& pVec1)
	{
		// Calculate the difference in each dimension
		float dx = pVec1.x - pVec0.x;
		float dy = pVec1.y - pVec0.y;

		// Calculate the distance using the Pythagorean theorem
		return sqrt(dx * dx + dy * dy);
	}

	/**************************************************************************/
	/*!
		\brief Calculates the squared distance between two vectors.
		\param pVec0 The first vector.
		\param pVec1 The second vector.
		\return The squared distance between the two vectors.
	*/
	/**************************************************************************/
	float Vector2DSquareDistance(const Vector2D& pVec0, const Vector2D& pVec1)
	{
		// Calculate the difference in each dimension
		float dx = pVec1.x - pVec0.x;
		float dy = pVec1.y - pVec0.y;

		// Calculate the distance using the Pythagorean theorem
		return (dx * dx + dy * dy);
	}

	/**************************************************************************/
	/*!
		\brief Calculates the dot product of two vectors.
		\param pVec0 The first vector.
		\param pVec1 The second vector.
		\return The dot product of the two vectors.
	*/
	/**************************************************************************/
	float Vector2DDotProduct(const Vector2D& pVec0, const Vector2D& pVec1)
	{
		// Implement the dot product formula
		return pVec0.x * pVec1.x + pVec0.y * pVec1.y;
	}

	/**************************************************************************/
	   /*!
		   \brief Calculates the magnitude of the cross product of two vectors.
		   \param pVec0 The first vector.
		   \param pVec1 The second vector.
		   \return The magnitude of the cross product.
	   */
	   /**************************************************************************/
	float Vector2DCrossProductMag(const Vector2D& pVec0, const Vector2D& pVec1) {
		return (pVec0.x * pVec1.y - pVec0.y * pVec1.x);
	}
}
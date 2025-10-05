/******************************************************************************/
/*!
\file		Vector3D.cpp
\author 	Jason Yeo Chee Kang, y.jasoncheekang, 2301505
\par    	email: y.jasoncheekang@digipen.edu
\date   	October 5, 2025
\brief		This file implements the operations for 3D vector manipulation,
			including addition, subtraction, scalar multiplication, division,
			normalization, and calculating length, distance, dot product, and
			cross product magnitude.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include "pch.h"
#include "Vector3D.h"
#include <cmath>

namespace UE_Vector {
	/**************************************************************************/
   /*!
	   \brief Constructs a 3D vector with initial x, y and z components.
	   \param _x The x component.
	   \param _y The y component.
	   \param _z The z component.
	*/
	/**************************************************************************/
	Vector3D::Vector3D(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {
	}

	/**************************************************************************/
	/*!
	  \brief Adds another vector to this vector.
	  \param rhs The right-hand side vector to add.
	  \return The resultant vector after addition.
	*/
	/**************************************************************************/
	Vector3D& Vector3D::operator += (const Vector3D& rhs) {
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;

		return *this;
	}

	/**************************************************************************/
	/*!
	   \brief Subtracts another vector from this vector.
	   \param rhs The right-hand side vector to subtract.
	   \return The resultant vector after subtraction.
	*/
	/**************************************************************************/
	Vector3D& Vector3D::operator -= (const Vector3D& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;

		return *this;
	}

	/**************************************************************************/
	/*!
	   \brief Multiplies this vector by a scalar.
	   \param rhs The scalar to multiply by.
	   \return The resultant vector after scalar multiplication.
	*/
	/**************************************************************************/
	Vector3D& Vector3D::operator *= (float rhs) {
		x *= rhs;
		y *= rhs;
		z *= rhs;

		return *this;
	}

	/**************************************************************************/
	/*!
		\brief Divides this vector by a scalar.
		\param rhs The scalar to divide by.
		\return The resultant vector after scalar division.
	*/
	/**************************************************************************/
	Vector3D& Vector3D::operator /= (float rhs) {
		x /= rhs;
		y /= rhs;
		z /= rhs;

		return *this;
	}

	/**************************************************************************/
	/*!
		\brief Negates this vector.
		\return The negated vector.
	*/
	/**************************************************************************/
	Vector3D Vector3D::operator -() const {
		return Vector3D(-x, -y, -z);
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
	Vector3D operator + (const Vector3D& lhs, const Vector3D& rhs) {
		return Vector3D(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
	}

	/******************************************************************************/
	/*!
		\brief Subtracts one vector from another.
		\param lhs The left-hand side vector.
		\param rhs The right-hand side vector to subtract from lhs.
		\return The result of vector subtraction.
	*/
	/******************************************************************************/
	Vector3D operator - (const Vector3D& lhs, const Vector3D& rhs) {
		return Vector3D(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
	}

	/******************************************************************************/
	/*!
		\brief Multiplies a vector by a scalar.
		\param lhs The vector to be scaled.
		\param rhs The scalar value.
		\return The result of scalar multiplication on the vector.
	*/
	/******************************************************************************/
	Vector3D operator * (const Vector3D& lhs, float rhs) {
		return Vector3D(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs);
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
	Vector3D operator * (float lhs, const Vector3D& rhs) {
		return Vector3D(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z);
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
	Vector3D operator / (const Vector3D& lhs, float rhs) {
		return Vector3D(lhs.x / rhs, lhs.y / rhs, lhs.z / rhs);
	}

	/**************************************************************************/
	/*!
		\brief Normalizes the provided vector.
		\param pResult The resultant normalized vector.
		\param pVec0 The vector to normalize.
	*/
	/**************************************************************************/
	void Vector3DNormalize(Vector3D& pResult, const Vector3D& pVec0) {
		float length = std::sqrt(pVec0.x * pVec0.x + pVec0.y * pVec0.y + pVec0.z * pVec0.z);
		if (length > 0.0f) {
			pResult.x = pVec0.x / length;
			pResult.y = pVec0.y / length;
			pResult.z = pVec0.z / length;
		}
		else {
			pResult.x = 0.0f;
			pResult.y = 0.0f;
			pResult.z = 0.0f;
		}
	}

	/**************************************************************************/
	/*!
		\brief Calculates the length of the vector.
		\param pVec0 The vector whose length is to be calculated.
		\return The length of the vector.
	*/
	/**************************************************************************/
	float Vector3DLength(const Vector3D& pVec0) {
		return std::sqrt(pVec0.x * pVec0.x + pVec0.y * pVec0.y + pVec0.z * pVec0.z);
	}

	/**************************************************************************/
	/*!
		\brief Calculates the squared length of the vector.
		\param pVec0 The vector whose squared length is to be calculated.
		\return The squared length of the vector.
	*/
	/**************************************************************************/
	float Vector3DSquareLength(const Vector3D& pVec0) {
		return (pVec0.x * pVec0.x + pVec0.y * pVec0.y + pVec0.z * pVec0.z);
	}

	/**************************************************************************/
	/*!
		\brief Calculates the distance between two vectors.
		\param pVec0 The first vector.
		\param pVec1 The second vector.
		\return The distance between the two vectors.
	*/
	/**************************************************************************/
	float Vector3DDistance(const Vector3D& pVec0, const Vector3D& pVec1) {
		float dx = pVec1.x - pVec0.x;
		float dy = pVec1.y - pVec0.y;
		float dz = pVec1.z - pVec0.z;

		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	/**************************************************************************/
	/*!
		\brief Calculates the squared distance between two vectors.
		\param pVec0 The first vector.
		\param pVec1 The second vector.
		\return The squared distance between the two vectors.
	*/
	/**************************************************************************/
	float Vector3DSquareDistance(const Vector3D& pVec0, const Vector3D& pVec1) {
		float dx = pVec1.x - pVec0.x;
		float dy = pVec1.y - pVec0.y;
		float dz = pVec1.z - pVec0.z;

		return (dx * dx + dy * dy + dz * dz);
	}

	/**************************************************************************/
	/*!
		\brief Calculates the dot product of two vectors.
		\param pVec0 The first vector.
		\param pVec1 The second vector.
		\return The dot product of the two vectors.
	*/
	/**************************************************************************/
	float Vector3DDotProduct(const Vector3D& pVec0, const Vector3D& pVec1) {
		return (pVec0.x * pVec1.x + pVec0.y * pVec1.y + pVec0.z * pVec1.z);
	}

	/**************************************************************************/
	/*!
		\brief Calculates the magnitude of the cross product of two vectors.
		\param pVec0 The first vector.
		\param pVec1 The second vector.
		\return The magnitude of the cross product.
	*/
	/**************************************************************************/
	void Vector3DCrossProduct(Vector3D& pResult, const Vector3D& pVec0, const Vector3D& pVec1) {
		pResult.x = (pVec0.y * pVec1.z) - (pVec0.z * pVec1.y);
		pResult.y = (pVec0.z * pVec1.x) - (pVec0.x * pVec1.z);
		pResult.z = (pVec0.x * pVec1.y) - (pVec0.y * pVec1.x);
	}
}
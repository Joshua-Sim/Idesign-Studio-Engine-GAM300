/******************************************************************************/
/*!
\file		Matrix3x3.cpp
\author 	Joshua Sim Yue Chen, joshuayuechen.sim, 2301283
\par    	email: joshuayuechen.sim@digipen.edu
\date   	March 25, 2024
\brief		This file contains implementations for 3x3 matrix operations that are
			crucial for 2D transformations, including translation, rotation, scaling,
			and their combinations. Functions provided include matrix multiplication,
			matrix-vector multiplication, and the generation of specific transformation
			matrices, supporting various operations necessary for 2D graphical transformations.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include "pch.h"
#include "Matrix3x3.h"

#include <string.h>
#include <cmath>


#define PIOVER180	0.0174532925199432f
#ifndef M_PI
#define PI 3.14159265358979323846
#endif

namespace UE_Vector
{
	/**************************************************************************/
		/*!
			\brief Constructs a Matrix3x3 from a pointer to an array of 9 floats.
			\param pArr Pointer to an array of 9 floats.
		*/
		/**************************************************************************/
	Matrix3x3::Matrix3x3(const float* pArr)
	{
		if (pArr)
		{
			for (int i = 0; i < 9; ++i)
			{
				m[i] = pArr[i];
			}
		}
	}

	/**************************************************************************/
	   /*!
		   \brief Constructs a Matrix3x3 using individual elements.
		   \param _00 to _22 Individual elements of the matrix.
	   */
	   /**************************************************************************/
	Matrix3x3::Matrix3x3(float _00, float _01, float _02,
		float _10, float _11, float _12,
		float _20, float _21, float _22)
		: m00(_00), m01(_01), m02(_02),
		m10(_10), m11(_11), m12(_12),
		m20(_20), m21(_21), m22(_22) {}

	/**************************************************************************/
	/*!
		\brief Assignment operator.
		\param rhs The right-hand side matrix to assign from.
		\return A reference to the assigned matrix.
	*/
	/**************************************************************************/
	Matrix3x3& Matrix3x3::operator=(const Matrix3x3& rhs)
	{
		if (this != &rhs)
		{
			for (int i = 0; i < 9; ++i)
			{
				m[i] = rhs.m[i];
			}
		}
		return *this;
	}

	/**************************************************************************/
   /*!
	   \brief Multiplies this matrix by another matrix.
	   \param rhs The right-hand side matrix to multiply with.
	   \return A reference to the product matrix.
   */
   /**************************************************************************/
	Matrix3x3& Matrix3x3::operator*=(const Matrix3x3& rhs)
	{
		Matrix3x3 temp;
		for (int row = 0; row < 3; ++row)
		{
			for (int col = 0; col < 3; ++col)
			{
				temp.m2[row][col] = m2[row][0] * rhs.m2[0][col] +
					m2[row][1] * rhs.m2[1][col] +
					m2[row][2] * rhs.m2[2][col];
			}
		}
		*this = temp;
		return *this;
	}

	/**************************************************************************/
	/*!
		\brief Multiplies two matrices.
		\param lhs The left-hand side matrix.
		\param rhs The right-hand side matrix.
		\return The product matrix.
	*/
	/**************************************************************************/
	Matrix3x3 operator*(const Matrix3x3& lhs, const Matrix3x3& rhs)
	{
		Matrix3x3 result;
		for (int row = 0; row < 3; ++row)
		{
			for (int col = 0; col < 3; ++col)
			{
				result.m2[row][col] = lhs.m2[row][0] * rhs.m2[0][col] +
					lhs.m2[row][1] * rhs.m2[1][col] +
					lhs.m2[row][2] * rhs.m2[2][col];
			}
		}
		return result;
	}


	/**************************************************************************/
	/*!
		\brief Multiplies a matrix by a vector.
		\param lhs The matrix.
		\param rhs The vector.
		\return The resulting vector.
	*/
	/**************************************************************************/
	Vector2D operator*(const Matrix3x3& lhs, const Vector2D& rhs)
	{
		return Vector2D(
			lhs.m00 * rhs.x + lhs.m01 * rhs.y + lhs.m02,
			lhs.m10 * rhs.x + lhs.m11 * rhs.y + lhs.m12);
	}

	/**************************************************************************/
	   /*!
		   \brief Sets a matrix to the identity matrix.
		   \param pResult The matrix to set to identity.
	   */
	   /**************************************************************************/
	void Mtx33Identity(Matrix3x3& pResult)
	{
		pResult = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	}

	/**************************************************************************/
	/*!
		\brief Creates a translation matrix.
		\param pResult The result matrix.
		\param x The x translation.
		\param y The y translation.
	*/
	/**************************************************************************/
	void Mtx33Translate(Matrix3x3& pResult, float x, float y)
	{
		Mtx33Identity(pResult);
		pResult.m02 = x;
		pResult.m12 = y;
	}

	/**************************************************************************/
	   /*!
		   \brief Creates a scaling matrix.
		   \param pResult The result matrix.
		   \param x The x scale factor.
		   \param y The y scale factor.
	   */
	   /**************************************************************************/
	void Mtx33Scale(Matrix3x3& pResult, float x, float y)
	{
		Mtx33Identity(pResult);
		pResult.m00 = x;
		pResult.m11 = y;
	}

	/**************************************************************************/
	  /*!
		  \brief Creates a rotation matrix from an angle in radians.
		  \param pResult The result matrix.
		  \param angle The rotation angle in radians.
	  */
	  /**************************************************************************/
	void Mtx33RotRad(Matrix3x3& pResult, float angle)
	{
		float c = cos(angle);
		float s = sin(angle);

		pResult = { c, -s, 0.0f, s, c, 0.0f, 0.0f, 0.0f, 1.0f };
	}
	/**************************************************************************/
	  /*!
		  \brief Creates a rotation matrix from an angle in degree.
		  \param pResult The result matrix.
		  \param angle The rotation angle in degree.
	  */
	  /**************************************************************************/
	void Mtx33RotDeg(Matrix3x3& pResult, float angle)
	{
		// Convert angle from degrees to radians
		float rad = static_cast<float>(angle * (PI / 180.0f));
		Mtx33RotRad(pResult, rad);
	}

	/**************************************************************************/
	 /*!
		 \brief Calculates the transpose of a given matrix.
		 \param pResult The matrix to store the transpose.
		 \param pMtx The source matrix.
	 */
	 /**************************************************************************/
	void Mtx33Transpose(Matrix3x3& pResult, const Matrix3x3& pMtx)
	{
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				pResult.m2[i][j] = pMtx.m2[j][i];
			}
		}
	}

	/**************************************************************************/
		/*!
			\brief Calculates the inverse of a given matrix.
			\param pResult Pointer to store the inverse matrix.
			\param determinant Pointer to store the determinant of the matrix.
			\param pMtx The source matrix.
		*/
		/**************************************************************************/
	void Mtx33Inverse(Matrix3x3* pResult, float* determinant, const Matrix3x3& pMtx) {
		if (!pResult) return; // Check if the result pointer is valid

		// Step 1: Calculate the determinant
		float det = pMtx.m00 * (pMtx.m11 * pMtx.m22 - pMtx.m21 * pMtx.m12) -
			pMtx.m01 * (pMtx.m10 * pMtx.m22 - pMtx.m20 * pMtx.m12) +
			pMtx.m02 * (pMtx.m10 * pMtx.m21 - pMtx.m20 * pMtx.m11);

		// Provide the determinant if requested
		if (determinant != nullptr) {
			*determinant = det;
		}

		// Check for zero determinant (no inverse exists)
		if (fabs(det) < 1e-9) {
			pResult = nullptr; // Indicate failure by setting pResult to NULL
			return; // No inverse
		}

		float invDet = 1.0f / det;

		// Step 2 & 3: Calculate cofactors for each element, then transpose (adjugate)
		Matrix3x3 inv; // Temp matrix to store inverse calculations
		inv.m00 = (pMtx.m11 * pMtx.m22 - pMtx.m21 * pMtx.m12) * invDet;
		inv.m01 = -(pMtx.m01 * pMtx.m22 - pMtx.m21 * pMtx.m02) * invDet;
		inv.m02 = (pMtx.m01 * pMtx.m12 - pMtx.m11 * pMtx.m02) * invDet;

		inv.m10 = -(pMtx.m10 * pMtx.m22 - pMtx.m20 * pMtx.m12) * invDet;
		inv.m11 = (pMtx.m00 * pMtx.m22 - pMtx.m20 * pMtx.m02) * invDet;
		inv.m12 = -(pMtx.m00 * pMtx.m12 - pMtx.m10 * pMtx.m02) * invDet;

		inv.m20 = (pMtx.m10 * pMtx.m21 - pMtx.m20 * pMtx.m11) * invDet;
		inv.m21 = -(pMtx.m00 * pMtx.m21 - pMtx.m20 * pMtx.m01) * invDet;
		inv.m22 = (pMtx.m00 * pMtx.m11 - pMtx.m10 * pMtx.m01) * invDet;

		// Step 4: Copy the calculated inverse matrix back to the result pointer
		*pResult = inv;
	}
}
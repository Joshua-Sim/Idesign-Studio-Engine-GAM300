/******************************************************************************/
/*!
\file		Matrix4x4.h
\author 	Jason Yeo Chee Kang, y.jasoncheekang, 2301505
\par    	email: y.jasoncheekang@digipen.edu
\date   	October 5, 2025
\brief  	This file includes the declaration of the Matrix3x3 structure

Copyright (C) 2023 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the prior
written consent of DigiPen Institute of Technology is prohibited.
*/
/******************************************************************************/

#pragma once
#include "Vector3D.h"


namespace UE_Vector
{
#ifdef _MSC_VER
    // Suppress warning: nonstandard extension used : nameless struct/union
#pragma warning( disable : 4201 )
#endif

/**************************************************************************/
/*!
   4x4 Matrix (row-major layout)
   Useful for 3D transforms including projection.
*/
/**************************************************************************/
    typedef union Matrix4x4
    {
        struct
        {
            float m00, m01, m02, m03;
            float m10, m11, m12, m13;
            float m20, m21, m22, m23;
            float m30, m31, m32, m33;
        };

        float m[16];
        float m2[4][4];

        // Constructors
        Matrix4x4()
            : m00(0.0f), m01(0.0f), m02(0.0f), m03(0.0f),
            m10(0.0f), m11(0.0f), m12(0.0f), m13(0.0f),
            m20(0.0f), m21(0.0f), m22(0.0f), m23(0.0f),
            m30(0.0f), m31(0.0f), m32(0.0f), m33(0.0f) {
        }

        Matrix4x4(const float* pArr);
        Matrix4x4(float _00, float _01, float _02, float _03,
            float _10, float _11, float _12, float _13,
            float _20, float _21, float _22, float _23,
            float _30, float _31, float _32, float _33);

        Matrix4x4& operator=(const Matrix4x4& rhs);

        // Do not change the following
        Matrix4x4(const Matrix4x4& rhs) = default;

        // Assignment operators
        Matrix4x4& operator *= (const Matrix4x4& rhs);

    } Matrix4x4, Mtx44;

#ifdef _MSC_VER
    // Restore warning state
#pragma warning( default : 4201 )
#endif

/**************************************************************************/
// Binary matrix operators
/**************************************************************************/
    Matrix4x4 operator * (const Matrix4x4& lhs, const Matrix4x4& rhs);

    /**************************************************************************/
    /*!
        Multiply matrix by a 4D vector (homogeneous), result is 4D raw product.
        Vector4 type not defined here; the function uses a float[4] convention
        or you can add a Vector4 type later. Overload for Vector3D below handles
        homogeneous divide.
    */
    /**************************************************************************/
    void Mtx44MulVec4(float outVec4[4], const Matrix4x4& m, const float inVec4[4]);

    /**************************************************************************/
    /*!
        Transform a 3D point by a 4x4 matrix (applies homogeneous w and divides).
        Result is Vector3D in Cartesian coordinates.
    */
    /**************************************************************************/
    Vector3D operator * (const Matrix4x4& m, const Vector3D& v); // Applies w = 1 transform and divides by w if needed

    /**************************************************************************/
    /*!
        Transform a 3D direction (no translation): treat w = 0 (no homogeneous divide).
    */
    /**************************************************************************/
    Vector3D Mtx44TransformDirection(const Matrix4x4& m, const Vector3D& v);

    /**************************************************************************/
    /*!
        Set to identity matrix
    */
    /**************************************************************************/
    void Mtx44Identity(Matrix4x4& pResult);

    /**************************************************************************/
    /*!
        Create translation matrix from x, y, z
    */
    /**************************************************************************/
    void Mtx44Translate(Matrix4x4& pResult, float x, float y, float z);

    /**************************************************************************/
    /*!
        Create scaling matrix from x, y, z
    */
    /**************************************************************************/
    void Mtx44Scale(Matrix4x4& pResult, float x, float y, float z);

    /**************************************************************************/
    /*!
        Create rotation matrix from Euler angles (radians), order: X, then Y, then Z
    */
    /**************************************************************************/
    void Mtx44RotRadXYZ(Matrix4x4& pResult, float rotX, float rotY, float rotZ);

    /**************************************************************************/
    /*!
        Create rotation matrix from Euler angles (degrees), order: X, then Y, then Z
    */
    /**************************************************************************/
    void Mtx44RotDegXYZ(Matrix4x4& pResult, float rotXDeg, float rotYDeg, float rotZDeg);

    /**************************************************************************/
    /*!
        Create rotation matrix about X axis (radians)
    */
    /**************************************************************************/
    void Mtx44RotX(Matrix4x4& pResult, float angleRad);

    /**************************************************************************/
    /*!
        Create rotation matrix about Y axis (radians)
    */
    /**************************************************************************/
    void Mtx44RotY(Matrix4x4& pResult, float angleRad);

    /**************************************************************************/
    /*!
        Create rotation matrix about Z axis (radians)
    */
    /**************************************************************************/
    void Mtx44RotZ(Matrix4x4& pResult, float angleRad);

    /**************************************************************************/
    /*!
        Create look-at view matrix. 'eye' is camera position, 'target' is look-at point,
        'up' is world up (usually {0,1,0}).
        Result is a view matrix transforming world->camera.
    */
    /**************************************************************************/
    void Mtx44LookAt(Matrix4x4& pResult, const Vector3D& eye, const Vector3D& target, const Vector3D& up);

    /**************************************************************************/
    /*!
        Create a perspective projection matrix (right-handed, depth in [-1,1] or [0,1]
        depends on convention document your chosen convention). Typical params:
        fovy (radians), aspect (width/height), znear, zfar.
    */
    /**************************************************************************/
    void Mtx44Perspective(Matrix4x4& pResult, float fovyRad, float aspect, float znear, float zfar);

    /**************************************************************************/
    /*!
        Create an orthographic projection matrix
    */
    /**************************************************************************/
    void Mtx44Ortho(Matrix4x4& pResult, float left, float right, float bottom, float top, float znear, float zfar);

    /**************************************************************************/
    /*!
        Transpose matrix
    */
    /**************************************************************************/
    void Mtx44Transpose(Matrix4x4& pResult, const Matrix4x4& pMtx);

    /**************************************************************************/
    /*!
        Compute determinant of 4x4 matrix
    */
    /**************************************************************************/
    float Mtx44Determinant(const Matrix4x4& pMtx);

    /**************************************************************************/
    /*!
        Compute inverse of matrix. If inversion fails (singular matrix), return false
        and pResult is untouched or set to identity based on your preference.
        This signature returns a boolean for success and optionally outputs determinant.
    */
    /**************************************************************************/
    bool Mtx44Inverse(Matrix4x4& pResult, float* determinant, const Matrix4x4& pMtx);

    /**************************************************************************/
    /*!
        Decompose a transform matrix into translation, rotation (as Euler angles),
        and scale. Returns true if decomposition succeeded.
        (Optional / helper)
    */
    /**************************************************************************/
    bool Mtx44DecomposeTRS(const Matrix4x4& m, Vector3D& outTranslation, Vector3D& outRotationEulerRad, Vector3D& outScale);

    /**************************************************************************/
    /*!
        Helpers: multiply matrix by scalar
    */
    /**************************************************************************/
    Matrix4x4 operator * (const Matrix4x4& lhs, float rhs);
    Matrix4x4 operator * (float lhs, const Matrix4x4& rhs);
    Matrix4x4 operator / (const Matrix4x4& lhs, float rhs);

} // namespace UE_Vector

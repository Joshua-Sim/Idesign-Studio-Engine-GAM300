/******************************************************************************/
/*!
\file		Vector3D.h
\author 	Jason Yeo Chee Kang, y.jasoncheekang, 2301505
\par    	email: y.jasoncheekang@digipen.edu
\date   	October 5, 2025
\brief		This file includes the declaration of the Vector3D structure

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#pragma once

namespace UE_Vector {
#ifdef _MSC_VER
    // Suppress warning: nonstandard extension used : nameless struct/union
#pragma warning( disable : 4201 )
#endif

/**************************************************************************/
/*!
   3D Vector type
*/
/**************************************************************************/
    typedef union Vector3D
    {
        struct
        {
            float x, y, z;
        };

        float m[3];

        // Constructors
        Vector3D() : x(0.0f), y(0.0f), z(0.0f) {}
        Vector3D(float _x, float _y, float _z);

        // Do not change the following
        Vector3D& operator=(const Vector3D& rhs) = default;
        Vector3D(const Vector3D& rhs) = default;

        // Assignment operators
        Vector3D& operator += (const Vector3D& rhs);
        Vector3D& operator -= (const Vector3D& rhs);
        Vector3D& operator *= (float rhs);
        Vector3D& operator /= (float rhs);

        // Unary operators
        Vector3D operator -() const;

    } Vector3D, Vec3, Point3D, Pt3;

#ifdef _MSC_VER
    // Restore warning state
#pragma warning( default : 4201 )
#endif

// Binary operators
    Vector3D operator + (const Vector3D& lhs, const Vector3D& rhs);
    Vector3D operator - (const Vector3D& lhs, const Vector3D& rhs);
    Vector3D operator * (const Vector3D& lhs, float rhs);
    Vector3D operator * (float lhs, const Vector3D& rhs);
    Vector3D operator / (const Vector3D& lhs, float rhs);

    /**************************************************************************/
    /*!
        Normalize: pResult will be the unit vector of pVec0
    */
    /**************************************************************************/
    void    Vector3DNormalize(Vector3D& pResult, const Vector3D& pVec0);

    /**************************************************************************/
    /*!
        Returns the length of pVec0
    */
    /**************************************************************************/
    float   Vector3DLength(const Vector3D& pVec0);

    /**************************************************************************/
    /*!
        Returns the square of pVec0's length (avoids sqrt)
    */
    /**************************************************************************/
    float   Vector3DSquareLength(const Vector3D& pVec0);

    /**************************************************************************/
    /*!
        Returns the distance between two 3D points
    */
    /**************************************************************************/
    float   Vector3DDistance(const Vector3D& pVec0, const Vector3D& pVec1);

    /**************************************************************************/
    /*!
        Returns the squared distance between two 3D points (avoids sqrt)
    */
    /**************************************************************************/
    float   Vector3DSquareDistance(const Vector3D& pVec0, const Vector3D& pVec1);

    /**************************************************************************/
    /*!
        Returns the dot product of pVec0 and pVec1
    */
    /**************************************************************************/
    float   Vector3DDotProduct(const Vector3D& pVec0, const Vector3D& pVec1);

    /**************************************************************************/
    /*!
        Returns the cross product of pVec0 and pVec1
    */
    /**************************************************************************/
    void    Vector3DCrossProduct(Vector3D& pResult, const Vector3D& pVec0, const Vector3D& pVec1);
}

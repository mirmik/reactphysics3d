/********************************************************************************
* ReactPhysics3D physics library, http://code.google.com/p/reactphysics3d/      *
* Copyright (c) 2010-2012 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

#ifndef CONSTRAINT_SOLVER_H
#define CONSTRAINT_SOLVER_H

// Libraries
#include "../constraint/Contact.h"
#include "../configuration.h"
#include "../constraint/Constraint.h"
#include <map>
#include <set>

// ReactPhysics3D namespace
namespace reactphysics3d {

// Declarations
class DynamicsWorld;

// Structure Impulse
struct Impulse {

    public:
        Vector3& linearImpulseBody1;
        Vector3& linearImpulseBody2;
        Vector3& angularImpulseBody1;
        Vector3& angularImpulseBody2;

        // Constructor
        Impulse(Vector3& linearImpulseBody1, Vector3& angularImpulseBody1,
                Vector3& linearImpulseBody2, Vector3& angularImpulseBody2)
            : linearImpulseBody1(linearImpulseBody1), angularImpulseBody1(angularImpulseBody1),
              linearImpulseBody2(linearImpulseBody2), angularImpulseBody2(angularImpulseBody2) {

        }
};

// Structure ContactPointConstraint
// Internal structure for a contact point constraint
struct ContactPointConstraint {

    decimal penetrationImpulse;             // Accumulated normal impulse
    decimal friction1Impulse;               // Accumulated impulse in the 1st friction direction
    decimal friction2Impulse;               // Accumulated impulse in the 2nd friction direction
    Vector3 normal;                         // Normal vector of the contact
    Vector3 frictionVector1;                // First friction vector in the tangent plane
    Vector3 frictionVector2;                // Second friction vector in the tangent plane
    Vector3 oldFrictionVector1;             // Old first friction vector in the tangent plane
    Vector3 oldFrictionVector2;             // Old second friction vector in the tangent plane
    Vector3 r1;                             // Vector from the body 1 center to the contact point
    Vector3 r2;                             // Vector from the body 2 center to the contact point
    Vector3 r1CrossT1;                      // Cross product of r1 with 1st friction vector
    Vector3 r1CrossT2;                      // Cross product of r1 with 2nd friction vector
    Vector3 r2CrossT1;                      // Cross product of r2 with 1st friction vector
    Vector3 r2CrossT2;                      // Cross product of r2 with 2nd friction vector
    Vector3 r1CrossN;                       // Cross product of r1 with the contact normal
    Vector3 r2CrossN;                       // Cross product of r2 with the contact normal
    decimal penetrationDepth;               // Penetration depth
    decimal restitutionBias;                // Velocity restitution bias
    decimal inversePenetrationMass;         // Inverse of the matrix K for the penenetration
    decimal inverseFriction1Mass;           // Inverse of the matrix K for the 1st friction
    decimal inverseFriction2Mass;           // Inverse of the matrix K for the 2nd friction
    decimal J_spBody1Penetration[6];
    decimal J_spBody2Penetration[6];
    decimal J_spBody1Friction1[6];
    decimal J_spBody2Friction1[6];
    decimal J_spBody1Friction2[6];
    decimal J_spBody2Friction2[6];
    decimal lowerBoundPenetration;
    decimal upperBoundPenetration;
    decimal lowerBoundFriction1;
    decimal upperBoundFriction1;
    decimal lowerBoundFriction2;
    decimal upperBoundFriction2;
    decimal errorPenetration;
    Contact* contact;                       // TODO : REMOVE THIS
    decimal b_Penetration;
    decimal B_spBody1Penetration[6];
    decimal B_spBody2Penetration[6];
    decimal B_spBody1Friction1[6];
    decimal B_spBody2Friction1[6];
    decimal B_spBody1Friction2[6];
    decimal B_spBody2Friction2[6];
};

// Structure ContactConstraint
struct ContactConstraint {

    // TODO : Use a constant for the number of contact points

    uint indexBody1;                        // Index of body 1 in the constraint solver
    uint indexBody2;                        // Index of body 2 in the constraint solver
    decimal massInverseBody1;               // Inverse of the mass of body 1
    decimal massInverseBody2;               // Inverse of the mass of body 2
    Matrix3x3 inverseInertiaTensorBody1;    // Inverse inertia tensor of body 1
    Matrix3x3 inverseInertiaTensorBody2;    // Inverse inertia tensor of body 2
    bool isBody1Moving;                     // True if the body 1 is allowed to move
    bool isBody2Moving;                     // True if the body 2 is allowed to move
    ContactPointConstraint contacts[4];     // Contact point constraints
    uint nbContacts;                        // Number of contact points
    decimal restitutionFactor;              // Mix of the restitution factor for two bodies
};
    
    
/*  -------------------------------------------------------------------
    Class ConstrainSolver :
        This class represents the constraint solver. The constraint solver
        is based on the theory from the paper "Iterative Dynamics with
        Temporal Coherence" from Erin Catto. We keep the same notations as
        in the paper. The idea is to construct a LCP problem and then solve
        it using a Projected Gauss Seidel (PGS) solver.
 
        The idea is to solve the following system for lambda :
                JM^-1J^T * lamdba - 1/dt * b_error + 1/dt * JV^1 + JM^-1F_ext >= 0
        
        By default, error correction using first world order projections (described
        by Erleben in section 4.16 in his PhD thesis "Stable, Robust, and
        Versatile Multibody Dynamics Animation") is used for very large penetration
        depths. Error correction with projection requires to solve another LCP problem
        that is simpler than the above one and by considering only the contact
        constraints. The LCP problem for error correction is the following one :
                J_contact M^-1 J_contact^T * lambda + 1/dt * -d >= 0
 
        where "d" is a vector with penetration depths. If the penetration depth of
       a given contact is not very large, we use the Baumgarte error correction (see
       the paper from Erin Catto).
        
    -------------------------------------------------------------------
*/
class ConstraintSolver {
    private:
        DynamicsWorld* world;                           // Reference to the world
        std::vector<Constraint*> activeConstraints;     // Current active constraints in the physics world
        bool isErrorCorrectionActive;                   // True if error correction (with world order) is active
        uint mNbIterations;                           // Number of iterations of the LCP solver
        uint nbConstraints;                             // Total number of constraints (with the auxiliary constraints)
        uint nbBodies;                                  // Current number of bodies in the physics world
        RigidBody* bodyMapping[NB_MAX_CONSTRAINTS][2];       // 2-dimensional array that contains the mapping of body reference
                                                        // in the J_sp and B_sp matrices. For instance the cell bodyMapping[i][j] contains
                                                        // the pointer to the body that correspond to the 1x6 J_ij matrix in the
                                                        // J_sp matrix. An integer body index refers to its index in the "bodies" std::vector
        decimal J_sp[NB_MAX_CONSTRAINTS][2*6];          // 2-dimensional array that correspond to the sparse representation of the jacobian matrix of all constraints
                                                        // This array contains for each constraint two 1x6 Jacobian matrices (one for each body of the constraint)
                                                        // a 1x6 matrix
        decimal B_sp[2][6*NB_MAX_CONSTRAINTS];          // 2-dimensional array that correspond to a useful matrix in sparse representation
                                                        // This array contains for each constraint two 6x1 matrices (one for each body of the constraint)
                                                        // a 6x1 matrix
        decimal b[NB_MAX_CONSTRAINTS];                  // Vector "b" of the LCP problem
        decimal d[NB_MAX_CONSTRAINTS];                  // Vector "d"
        Matrix3x3 Minv_sp_inertia[NB_MAX_BODIES];       // 3x3 world inertia tensor matrix I for each body (from the Minv_sp matrix)
        decimal Minv_sp_mass_diag[NB_MAX_BODIES];       // Array that contains for each body the inverse of its mass
                                                        // This is an array of size nbBodies that contains in each cell a 6x6 matrix
        Vector3* mLinearVelocities;                     // Array of constrained linear velocities
        Vector3* mAngularVelocities;                    // Array of constrained angular velocities
        decimal mTimeStep;                              // Current time step

        // Contact constraints
        ContactConstraint* mContactConstraints;

        // Number of contact constraints
        uint mNbContactConstraints;

        // Constrained bodies
        std::set<RigidBody*> mConstraintBodies;

        // Map body to index
        std::map<RigidBody*, uint> mMapBodyToIndex;


        void initialize();                              // Initialize the constraint solver before each solving
        void initializeBodies();                        // Initialize bodies velocities
        void initializeContactConstraints(decimal dt);                // Fill in all the matrices needed to solve the LCP problem
        void computeMatrixB_sp();                       // Compute the matrix B_sp
        void cacheLambda();                             // Cache the lambda values in order to reuse them in the next step to initialize the lambda vector
        void warmStart();                          // Compute the vector a used in the solve() method
        void solveLCP();                                // Solve a LCP problem using Projected-Gauss-Seidel algorithm

        // Apply an impulse to the two bodies of a constraint
        void applyImpulse(const Impulse& impulse, const ContactConstraint& constraint);

        // Compute the collision restitution factor from the restitution factor of each body
        decimal computeMixRestitutionFactor(const RigidBody *body1, const RigidBody *body2) const;

   public:
        ConstraintSolver(DynamicsWorld* world);                         // Constructor
        virtual ~ConstraintSolver();                                   // Destructor
        void solve(decimal dt);                                         // Solve the current LCP problem
        bool isConstrainedBody(RigidBody* body) const;                 // Return true if the body is in at least one constraint
        Vector3 getConstrainedLinearVelocityOfBody(RigidBody *body);        // Return the constrained linear velocity of a body after solving the LCP problem
        Vector3 getConstrainedAngularVelocityOfBody(RigidBody* body);       // Return the constrained angular velocity of a body after solving the LCP problem
        void cleanup();                                                 // Cleanup of the constraint solver
        void setNbLCPIterations(uint mNbIterations);                     // Set the number of iterations of the LCP solver
};

// Return true if the body is in at least one constraint
inline bool ConstraintSolver::isConstrainedBody(RigidBody* body) const {
    return mConstraintBodies.count(body) == 1;
}

// Return the constrained linear velocity of a body after solving the LCP problem
inline Vector3 ConstraintSolver::getConstrainedLinearVelocityOfBody(RigidBody* body) {
    assert(isConstrainedBody(body));
    uint indexBodyArray = mMapBodyToIndex[body];
    return mLinearVelocities[indexBodyArray];
}

// Return the constrained angular velocity of a body after solving the LCP problem
inline Vector3 ConstraintSolver::getConstrainedAngularVelocityOfBody(RigidBody *body) {
    assert(isConstrainedBody(body));
    uint indexBodyArray = mMapBodyToIndex[body];
    return mAngularVelocities[indexBodyArray];
}

// Cleanup of the constraint solver
inline void ConstraintSolver::cleanup() {
    mMapBodyToIndex.clear();
    mConstraintBodies.clear();
    activeConstraints.clear();

    if (mContactConstraints != 0) {
        delete[] mContactConstraints;
        mContactConstraints = 0;
    }
    if (mLinearVelocities != 0) {
        delete[] mLinearVelocities;
        mLinearVelocities = 0;
    }
    if (mAngularVelocities != 0) {
        delete[] mAngularVelocities;
        mAngularVelocities = 0;
    }
}

// Set the number of iterations of the LCP solver
inline void ConstraintSolver::setNbLCPIterations(uint nbIterations) {
    mNbIterations = nbIterations;
}

// Compute the collision restitution factor from the restitution factor of each body
inline decimal ConstraintSolver::computeMixRestitutionFactor(const RigidBody* body1,
                                                             const RigidBody* body2) const {
    decimal restitution1 = body1->getRestitution();
    decimal restitution2 = body2->getRestitution();

    // Return the largest restitution factor
    return (restitution1 > restitution2) ? restitution1 : restitution2;
}

} // End of ReactPhysics3D namespace

#endif

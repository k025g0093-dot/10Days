#pragma once
#include <Jolt/Jolt.h>                              // 基本型（Vec3, BodyID など）
#include <Jolt/RegisterTypes.h>                     // RegisterTypes()
#include <Jolt/Core/Factory.h>                      // Factory
#include <Jolt/Core/TempAllocator.h>                // TempAllocatorImpl
#include <Jolt/Core/JobSystemThreadPool.h>          // JobSystemThreadPool
#include <Jolt/Physics/PhysicsSystem.h>             // PhysicsSystem
#include <Jolt/Physics/Body/BodyCreationSettings.h> // BodyCreationSettings
#include <Jolt/Physics/Body/BodyActivationListener.h>// BodyActivationListener
#include <Jolt/Physics/Collision/Shape/BoxShape.h>  // 床用BoxShape
#include <Jolt/Physics/Collision/ContactListener.h>  // 衝突通知
#include <Jolt/Physics/Collision/Shape/SphereShape.h>// SphereCollider用
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>// ConvexHullCollider用
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Body/BodyInterface.h>    // これがないとBody操作できない


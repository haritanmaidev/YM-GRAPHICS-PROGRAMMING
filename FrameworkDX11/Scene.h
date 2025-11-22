// MIT License
// Copyright (c) 2025 David White
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// Scene class for encapsulating the responsibily for storing / initialising the world and whatever is in it

#pragma once

#include "constants.h"
#include "Camera.h"
#include <d3d11_1.h>
#include <vector>
#include "wrl.h"
#include "structures.h"
#include "scenegraph.h"

class DX11Renderer;

class Scene
{
public:

	Scene() {}
	~Scene() {}
	
	HRESULT		init(HWND hwnd, const Microsoft::WRL::ComPtr<ID3D11Device>& device, const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& context, DX11Renderer* renderer);
	void		cleanUp();
	Camera*		getCamera() { return m_pCamera; }
	void		update(const float deltaTime);
	const LightPropertiesConstantBuffer& getLightProperties() { return m_lightProperties; }

private:
	void setupLightProperties();

public:
	Camera* m_pCamera;
	
	Microsoft::WRL::ComPtr <ID3D11Device>			m_pd3dDevice;
	Microsoft::WRL::ComPtr <ID3D11DeviceContext>	m_pImmediateContext;
	Microsoft::WRL::ComPtr <ID3D11Buffer>			m_pConstantBuffer;
	Microsoft::WRL::ComPtr <ID3D11Buffer>			m_pLightConstantBuffer;
	Microsoft::WRL::ComPtr <ID3D11Buffer>			m_pCustomConstantBuffer;

	DX11Renderer* m_DX11Renderer = nullptr;
	LightPropertiesConstantBuffer m_lightProperties;
	IRenderingContext m_ctx;
	SceneGraph m_sceneobject;
	SceneGraph m_sceneobjectSkinning;

	//------------------------ [[WEEK 5,6 - ANIMATION VALUES N STUFF SECTION]] ------------------------
	HRESULT AnimationHierarchyInit(); //Custom Animation Hierarchy Init Function
	Animation m_AnimationClass;

	//Parent Matrix Values
	DirectX::XMFLOAT3 m_startPosParent = { -3.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_endPosParent = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 m_startRotParent;
	DirectX::XMFLOAT4 m_endRotParent;

	//Child one Matrix Values
	DirectX::XMFLOAT3 m_startPosChild1 = {0.0f, 3.0f, 0.0f};
	DirectX::XMFLOAT3 m_endPosChild1 = { 0.0f, -3.0f, 0.0f };
	DirectX::XMFLOAT4 m_startRotChild1;
	DirectX::XMFLOAT4 m_endRotChild1;

	//Child two Matrix Values
	DirectX::XMFLOAT3 m_startPosChild2 = { 3.5f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_endPosChild2 = { 0.f, 0.f, 0.f };
	DirectX::XMFLOAT4 m_startRotChild2;
	DirectX::XMFLOAT4 m_endRotChild2;

	DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixIdentity();
	float animationTimer = 0.0f;

	//------------------------ [[START OF WEEK 7.1 - SKELETON STUFF SECTION]] ------------------------
	//Made this sceneobject struct to the previously global variables
	//Animation class holds a list of struct JointAnimValues, which holds values of each joint per different anim clip
	struct SceneObject
	{
		Skeleton m_robotArmSkeleton;
		std::vector<SceneNode*> m_armSegmentNodes;
		std::vector<Animation> m_robotArmAnimations;
		SceneGraph m_HierarchicalObject;

		int currentAnimIndex = 0;
		bool pendingApply = false;
	};

	std::vector<SceneObject> m_hierarchicalObjects;

	//Week 7.1 Custom Skeleton Arm Init Function
	void SkeletonArmInit(int sceneObjCount); 
	Animation CreateRobotArmAnimationDefault(SceneObject& currentObject, std::string name);
	Animation CreateRobotArmAnimationTriangle(SceneObject& currentObject, std::string name);
	Animation CreateRobotArmAnimationSpin(SceneObject& currentObject, std::string name);
	Animation CreateRobotArmAnimationDingDong(SceneObject& currentObject, std::string name);
	Animation CreateRobotArmAnimationCluster(SceneObject& currentObject, std::string name);
	Animation CreateRobotArmAnimationLaunch(SceneObject& currentObject, std::string name);
	Animation CreateRobotArmAnimationFromExisting(SceneObject& currentObject, int index);
	void CreateAnimationSampler(SceneObject& currentObject, int nodeIndex, Animation* anim, const DirectX::XMFLOAT3& startP,const DirectX::XMFLOAT3& endP, const DirectX::XMFLOAT3& startR, const DirectX::XMFLOAT3& endR,float startAngle, float endAngle);

	//Week 7.2 Custom Skeleton Pole & Fox
	//Pole
	void SkeletonPoleInit(); 
	Animation* CreatePoleAnimation(Skeleton* skeleton);
	void CreateWaveAnimationSamplerForPole(int nodeIndex, Animation* anim, Skeleton* skeleton);
	
	//Fox
	void FoxInit(int foxCount);

	struct Fox
	{
		SceneGraph m_foxSceneObject;

		int foxAnimationIndex = 0;
		int lastAnimIndex = -1;
		float foxAngle = 0.0f; //Current angle of fox
		float foxSpeed = 0.0f; //Current speed of fox

		float foxRunRadius = 4.0f;
		float foxRunSpeedMax = XM_2PI / 4.0f; //Fox's faster speed
		float foxRunSpeedMin = XM_2PI / 8.0f;

		float foxWalkRadius = 6.0f;
		float foxWalkSpeedMax = XM_2PI / 14.0f; //Fox's slower speed
		float foxWalkSpeedMin = XM_2PI / 16.0f;

		float speedTimer = 0.0f; //speedTimer is the t value for lerping between fox's speed1 and speed2
		int speedSign = 1;
	};

	std::vector<Fox> m_foxObjects;
	//------------------------ [[END OF WEEK 7 - SKELETON STUFF SECTION]] ------------------------

private:
	ID3D11ShaderResourceView* m_pTextureDiffuse;
	ID3D11ShaderResourceView* m_pTextureNormal;
	ID3D11ShaderResourceView* m_pTextureMetallic;
	ID3D11ShaderResourceView* m_pTextureRoughness;
	ID3D11ShaderResourceView* m_pTextureAmbientOcclusion;
	ID3D11ShaderResourceView* m_pTextureSpecularIBL;
	ID3D11ShaderResourceView* m_pTextureDiffuseIBL;

	ID3D11SamplerState* m_pSamplerLinear;
};


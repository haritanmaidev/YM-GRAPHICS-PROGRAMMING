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

	LightPropertiesConstantBuffer m_lightProperties;
	IRenderingContext m_ctx;
	SceneGraph m_sceneobject;
	SceneGraph m_sceneobjectSkinning;
	SceneGraph m_sceneobjectFox;

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

	//------------------------ [[START OF WEEK 7 - SKELETON STUFF SECTION]] ------------------------
	//Week 7.1 Custom Skeleton Arm Init Function
	void SkeletonArmInit(); 
	Animation CreateWaveAnimation();
	void CreateWaveAnimationSampler(int nodeIndex, Animation* anim, const DirectX::XMFLOAT3& startP,const DirectX::XMFLOAT3& endP, const DirectX::XMFLOAT3& startR, const DirectX::XMFLOAT3& endR,float startAngle, float endAngle);

	Skeleton m_robotArmSkeleton;
	std::vector<SceneNode*> m_armSegmentNodes;
	std::vector<Animation> m_robotArmAnimations;

	struct NodeAnimationValues
	{
		DirectX::XMFLOAT3 startPos = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 endPos = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 startRot = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 endRot = { 0.0f, 0.0f, 0.0f };
		float startAngle = 0.0f;
		float endAngle = DirectX::XM_PIDIV2;
	};

	std::vector<NodeAnimationValues> m_nodeAnimValues;

	//RobotArm IMGUI
	//Microsoft::WRL::ComPtr <ID3D11Buffer>m_pConstantBufferAnimation;
	//ConstantBufferAnimation m_AnimationBufferValues;
	//ConstantBufferAnimation& GetAnimationBufferValues() { return m_AnimationBufferValues; }

	//Week 7.2 Custom Skeleton Pole & Fox
	void SkeletonPoleInit(); 
	Animation* CreatePoleAnimation(Skeleton* skeleton);
	void CreateWaveAnimationSamplerForPole(int nodeIndex, Animation* anim, Skeleton* skeleton);
	
	void FoxInit();
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


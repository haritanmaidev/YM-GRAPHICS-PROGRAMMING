#include "Scene.h"
#include "DDSTextureLoader.h"

//Helper for Translation
DirectX::XMFLOAT3 BakeTranslationOntoBindPose(const DirectX::XMMATRIX& bindPose, const DirectX::XMFLOAT3& animTranslation)
{
    //1. Create anim matrix from the vector
    DirectX::XMMATRIX animMatrix = DirectX::XMMatrixTranslation(animTranslation.x, animTranslation.y, animTranslation.z);

    //2. Bake animation onto bind pose
    DirectX::XMMATRIX finalLocalMatrix = animMatrix * bindPose;

    //3. Extract and return the final position
    DirectX::XMFLOAT3 finalPosition;
    DirectX::XMStoreFloat3(&finalPosition, finalLocalMatrix.r[3]);
    return finalPosition;
}

//Helper for Rotation
DirectX::XMFLOAT4 BakeRotationOntoBindPose(const DirectX::XMMATRIX& bindPose, const DirectX::XMFLOAT3& axis, float angleRadians)
{
    //1. Create anim matrix from axis and angle
    DirectX::XMMATRIX animMatrix = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&axis), angleRadians);

    //2. Bake the animation onto bind pose
    DirectX::XMMATRIX finalLocalMatrix = animMatrix * bindPose;

    //3. Decompose to safely extractr and return the final rotation quat
    DirectX::XMVECTOR scale, finalRotationQuat, translation;
    DirectX::XMMatrixDecompose(&scale, &finalRotationQuat, &translation, finalLocalMatrix);
    DirectX::XMFLOAT4 finalRotation;
    DirectX::XMStoreFloat4(&finalRotation, finalRotationQuat);

    return finalRotation;
}

//Helper for Scale
DirectX::XMFLOAT3 BakeScaleOntoBindPose(const DirectX::XMMATRIX& bindPose, const DirectX::XMFLOAT3& animScale)
{
    //1. Create anim matrix from the vector.
    DirectX::XMMATRIX animMatrix = DirectX::XMMatrixScaling(animScale.x, animScale.y, animScale.z);

    //2. Bake the animation onto the bind pose.
    DirectX::XMMATRIX finalLocalMatrix = animMatrix * bindPose;

    //3. Decompose to safely extract and return the final scale vector.
    DirectX::XMVECTOR finalScale, rotation, translation;
    DirectX::XMMatrixDecompose(&finalScale, &rotation, &translation, finalLocalMatrix);
    DirectX::XMFLOAT3 finalScaleVec;
    DirectX::XMStoreFloat3(&finalScaleVec, finalScale);
    return finalScaleVec;
}

HRESULT Scene::init(HWND hwnd, const Microsoft::WRL::ComPtr<ID3D11Device>& device, const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& context, DX11Renderer* renderer)
{
	m_pd3dDevice = device;
	m_pImmediateContext = context;

    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    HRESULT hr;

    m_ctx.Init(device.Get(), context.Get(), renderer);
    
    //-------------------- [[WEEK 5 6 - ANIMATION HIERARCHY INIT HERE]] --------------------
    //if (bool animInit = AnimationHierarchyInit() == E_FAIL) return E_FAIL;

    //-------------------- [[WEEK 7.1 - SKELETON ARM INIT HERE]] --------------------
    SkeletonArmInit(2);

    //-------------------- [[WEEK 7.2 - SIMPLERIG/POLE AND FOX INIT HERE]] --------------------
	SkeletonPoleInit(); 
    FoxInit();

    m_pCamera = new Camera(XMFLOAT3(8, 0, 2), XMFLOAT3(-1, 0, 0), XMFLOAT3(0.0f, 1.0f, 0.0f), width, height); //Fox Cam
    //m_pCamera = new Camera(XMFLOAT3(0, 0, -10), XMFLOAT3(0, 0, 1), XMFLOAT3(0.0f, 1.0f, 0.0f), width, height); //Pole PreSkin Cam
    //m_pCamera = new Camera(XMFLOAT3(0, 0, -6), XMFLOAT3(0, 0, 1), XMFLOAT3(0.0f, 1.0f, 0.0f), width, height); // RoboArm Cam

    // Create the constant buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = m_pd3dDevice->CreateBuffer(&bd, nullptr, &m_pConstantBuffer);
    if (FAILED(hr))
        return hr;

    setupLightProperties();

    // Create the light constant buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(LightPropertiesConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = m_pd3dDevice->CreateBuffer(&bd, nullptr, &m_pLightConstantBuffer);
    if (FAILED(hr))
        return hr;

    // load and setup textures
    hr = CreateDDSTextureFromFile(m_pd3dDevice.Get(), L"Resources\\rusty_metal_04_diff.dds", nullptr, &m_pTextureDiffuse);
    if (FAILED(hr))
        return hr;

    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = m_pd3dDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear);

    return S_OK;
}

void Scene::cleanUp()
{
    delete m_pCamera;
}

void Scene::setupLightProperties()
{
    Light light;
    light.Enabled = static_cast<int>(true);
    light.LightType = PointLight;
    light.Color = XMFLOAT4(1,1,1,1);
    light.SpotAngle = XMConvertToRadians(45.0f);
    light.ConstantAttenuation = 1.0f;
    light.LinearAttenuation = 1;
    light.QuadraticAttenuation = 1;

    // set up the light
    XMFLOAT4 LightPosition(m_pCamera->getPosition().x, m_pCamera->getPosition().y, m_pCamera->getPosition().z, 1);
    light.Position = LightPosition;
    
    m_lightProperties.EyePosition = LightPosition;
    m_lightProperties.Lights[0] = light;
}

//-------------------- ===[[[[WEEK 6 Init Definitions here]]]]=== --------------------
HRESULT Scene::AnimationHierarchyInit()
{
    //---------------------|||[[[[[ HIERARCHY CREATION START ]]]]]|||---------------------
//    //PARENT NODE INITIALIZATION
    SceneNode* sunNode = m_sceneobject.CreateRootNode();
    if (!sunNode) return E_FAIL;
//    sunNode->LoadSphere(m_ctx);
//    sunNode->AddTranslation({ 0.0, 0, 2.0 });
//    //Parent node: translation logic
//    AnimationChannel channel0translation;
//    AnimationSampler sampler0Translation;
//    sampler0Translation.vec3_values.push_back(m_startPosParent);// Setting position keyframes
//    sampler0Translation.vec3_values.push_back(m_endPosParent);
//    sampler0Translation.timestamps.push_back(0.0f);// Setting time keyframes for each keyframe
//    sampler0Translation.timestamps.push_back(2.0f);
//    m_AnimationClass.m_samplers.push_back(sampler0Translation);
//    m_AnimationClass.m_channels.push_back({ channel0translation.TRANSLATION, 0, 0 });
//    //Parent node: rotation logic
//    AnimationChannel channel1Rotation;
//    AnimationSampler sampler1Rotation;
//    DirectX::XMStoreFloat4(&m_startRotParent, DirectX::XMQuaternionIdentity());
//    DirectX::XMStoreFloat4(&m_endRotParent, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XM_PI));
//    sampler1Rotation.vec4_values.push_back(m_startRotParent);//Rotation keyframes
//    sampler1Rotation.vec4_values.push_back(m_endRotParent);
//    sampler1Rotation.timestamps.push_back(0.0f); // Rotation timestamps of each keyframe
//    sampler1Rotation.timestamps.push_back(4.0f);
//    m_AnimationClass.m_samplers.push_back(sampler1Rotation);
//    m_AnimationClass.m_channels.push_back({ channel1Rotation.ROTATION, 0, 1 });
//
//#pragma region child 1
//    //CHILD NODE INITIALIZATION
//    SceneNode* planetOneNode = sunNode->CreateChildNode();
//    if (!planetOneNode) return E_FAIL;
//    if (!planetOneNode->LoadSphere(m_ctx));
//    planetOneNode->AddTranslation({ 0, 2.5, 0 });
//    //Child one node: translation logic
//    AnimationChannel channel2translation;
//    AnimationSampler sampler2Translation;
//    sampler2Translation.vec3_values.push_back(m_startPosChild1);
//    sampler2Translation.vec3_values.push_back({ m_endPosChild1 });
//    sampler2Translation.timestamps.push_back(0.0f);
//    sampler2Translation.timestamps.push_back(2.0f);
//    m_AnimationClass.m_samplers.push_back(sampler2Translation);
//    m_AnimationClass.m_channels.push_back({ channel2translation.TRANSLATION, 1, 2 });
//    //Child one node: rotation logic
//    AnimationChannel channel3Rotation;
//    AnimationSampler sampler3Rotation;
//    DirectX::XMStoreFloat4(&m_startRotChild1, DirectX::XMQuaternionIdentity());
//    DirectX::XMStoreFloat4(&m_endRotChild1, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1.0, 1.0f, 0.0f, 0.0f), 720));
//    sampler3Rotation.vec4_values.push_back({ m_startRotChild1 });
//    sampler3Rotation.vec4_values.push_back({ m_endRotChild1 });
//    sampler3Rotation.timestamps.push_back(0.0f);
//    sampler3Rotation.timestamps.push_back(4.0f);
//    m_AnimationClass.m_samplers.push_back(sampler3Rotation);
//    m_AnimationClass.m_channels.push_back({ channel3Rotation.ROTATION, 1, 3 });
//#pragma endregion
//
//#pragma region child 2
//    //CHILD 2 NODE INITIALIZATION
//    SceneNode* planetTwoNode = sunNode->CreateChildNode();
//    if (!planetTwoNode) return E_FAIL;
//    if (!planetTwoNode->LoadSphere(m_ctx));
//    planetTwoNode->AddTranslation({ 0, 2.5, 0 });
//    //Child two node: translation logic
//    AnimationChannel channel4translation;
//    AnimationSampler sampler4Translation;
//    sampler4Translation.vec3_values.push_back(m_startPosChild2);
//    sampler4Translation.vec3_values.push_back({ m_startPosChild2 });
//    sampler4Translation.timestamps.push_back(0.0f);
//    sampler4Translation.timestamps.push_back(2.0f);
//    m_AnimationClass.m_samplers.push_back(sampler4Translation);
//    m_AnimationClass.m_channels.push_back({ channel4translation.TRANSLATION, 2, 4 });
//    //Child two node: rotation logic
//    AnimationChannel channel5otation;
//    AnimationSampler sampler5Rotation;
//    DirectX::XMStoreFloat4(&m_startRotChild2, DirectX::XMQuaternionIdentity());
//    DirectX::XMStoreFloat4(&m_endRotChild2, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1.f, 0.0f, 0.0f, 0.0f),0));
//    sampler5Rotation.vec4_values.push_back({ m_startRotChild2 });
//    sampler5Rotation.vec4_values.push_back({ m_endRotChild2 });
//    sampler5Rotation.timestamps.push_back(0.0f);
//    sampler5Rotation.timestamps.push_back(2.0f);
//    m_AnimationClass.m_samplers.push_back(sampler5Rotation);
//    m_AnimationClass.m_channels.push_back({ channel5otation.ROTATION, 2, 5 });
#pragma endregion
    //---------------------|||[[[[[ HIERARCHY CREATION END ]]]]]|||---------------------
}

//-------------------- ===[[[[WEEK 7.1 Init & Anim Definitions here]]]]=== --------------------
void Scene::SkeletonArmInit(int sceneObjCount)
{  
    m_hierarchicalObjects.resize(sceneObjCount);

    for (int i = 0; i < sceneObjCount; i++) 
    {
		SceneObject& currentObject = m_hierarchicalObjects[i];

        const float segmentLength = 2.0f;

        //--- 1. Build the Skeleton Hierarchy ---
        DirectX::XMFLOAT4X4 shoulderTransform;
        DirectX::XMStoreFloat4x4(&shoulderTransform, DirectX::XMMatrixIdentity());
        int shoulderIndex = currentObject.m_robotArmSkeleton.AddJoint(-1, shoulderTransform);

        //Scale the elbow and hand nodes in turn
        DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(0.75, 0.75, 0.75);
        DirectX::XMFLOAT4X4 elbowTransform;
        DirectX::XMStoreFloat4x4(&elbowTransform, scale * DirectX::XMMatrixTranslation(0.0f, segmentLength, 0.0f));
        int elbowIndex = currentObject.m_robotArmSkeleton.AddJoint(shoulderIndex, elbowTransform);

        //Hand
        DirectX::XMFLOAT4X4 handTransform;
        DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(0.0f, segmentLength, 0.0f);
        DirectX::XMStoreFloat4x4(&handTransform, scale * translation);
        int handIndex = currentObject.m_robotArmSkeleton.AddJoint(elbowIndex, handTransform);

        //root nodes migh change as the vector is resized, so this must be done first
        for (int i = 0; i < currentObject.m_robotArmSkeleton.GetBoneCount(); i++)
        {
            currentObject.m_HierarchicalObject.CreateRootNode();
        }

        //--- 2. Create the Visible Geometry for Each Joint ---
        for (int i = 0; i < currentObject.m_robotArmSkeleton.GetBoneCount(); i++)
        {
            SceneNode* segmentNode = currentObject.m_HierarchicalObject.GetRootNode(i);
            segmentNode->LoadSphere(m_ctx);
            //segmentNode->AddTranslation({ i * 3.0f, 0, 0 });
            currentObject.m_armSegmentNodes.push_back(segmentNode);
        }

        //--- 3. Create the Animation for the Skeleton --
        if (i == 1) 
        {
            currentObject.m_robotArmAnimations.push_back(CreateRobotArmAnimationDingDong(currentObject, "DingDong Animation 1"));
            currentObject.m_robotArmAnimations.push_back(CreateRobotArmAnimationCluster(currentObject, "Cluster Animation 2"));
            currentObject.m_robotArmAnimations.push_back(CreateRobotArmAnimationLaunch(currentObject, "Launch Animation 3"));
        }
        else 
        {
            currentObject.m_robotArmAnimations.push_back(CreateRobotArmAnimationDefault(currentObject, "Default Animation 1"));
            currentObject.m_robotArmAnimations.push_back(CreateRobotArmAnimationTriangle(currentObject, "Triangle Animation 2"));
            currentObject.m_robotArmAnimations.push_back(CreateRobotArmAnimationSpin(currentObject, "Spin Animation 3"));
        }
    }
}

Animation Scene::CreateRobotArmAnimationDefault(SceneObject& currentObject, std::string name)
{
    //Create animation clip instance to which values will be added
    Animation fakeClip;

    //Make sure robot animations array has space for new clip
    int prevAnimClipSize = currentObject.m_robotArmAnimations.size();
    currentObject.m_robotArmAnimations.resize(prevAnimClipSize + 1);
	int currentAnimClipIndex = currentObject.m_robotArmAnimations.size() - 1;

    //Make sure joint anim values array of the current animation clip has space for all the joints
    int jointCount = currentObject.m_robotArmSkeleton.GetBoneCount();
    currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues.resize(jointCount);

	//Set the name and joint anim values array of the new animation clip
    fakeClip.m_name = name;
    fakeClip.m_jointAnimValues = currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues;
    
    //Init default values for each joint
    if (jointCount > 0) {
        fakeClip.m_jointAnimValues[0].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 2.0f };
        fakeClip.m_jointAnimValues[0].endPos = DirectX::XMFLOAT3{ 0.0f, 2.0f, 2.0f };
        fakeClip.m_jointAnimValues[0].startRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[0].endRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[0].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[0].endAngle = DirectX::XM_PIDIV2;
    }
    if (jointCount > 1) {
        fakeClip.m_jointAnimValues[1].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].endPos = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].startRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[1].endRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[1].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[1].endAngle = DirectX::XM_PI;
    }
    if (jointCount > 2) {
        fakeClip.m_jointAnimValues[2].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].endPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f };
        fakeClip.m_jointAnimValues[2].startRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[2].endRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[2].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[2].endAngle = 0.0f;
    }

    Animation newAnim;
    newAnim.m_name = name;
	newAnim.m_jointAnimValues = fakeClip.m_jointAnimValues;

	int currentJointCount = newAnim.m_jointAnimValues.size();
    for (int i = 0; i < currentJointCount; i++) 
    {
		auto& currentJointValues = newAnim.m_jointAnimValues[i];

		CreateAnimationSampler(currentObject, i, &newAnim,
            currentJointValues.startPos, currentJointValues.endPos,
            currentJointValues.startRot, currentJointValues.endRot,
            currentJointValues.startAngle, currentJointValues.endAngle);
    }

    //Resizing here since the default values above creates an extra empty clip
    currentObject.m_robotArmAnimations.resize(currentAnimClipIndex);

    return newAnim;
}

Animation Scene::CreateRobotArmAnimationTriangle(SceneObject& currentObject, std::string name)
{
    //Create animation clip instance to which values will be added
    Animation fakeClip;

    //Make sure robot animations array has space for new clip
    int prevAnimClipSize = currentObject.m_robotArmAnimations.size();
    currentObject.m_robotArmAnimations.resize(prevAnimClipSize + 1);
    int currentAnimClipIndex = currentObject.m_robotArmAnimations.size() - 1;

    //Make sure joint anim values array of the current animation clip has space for all the joints
    int jointCount = currentObject.m_robotArmSkeleton.GetBoneCount();
    currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues.resize(jointCount);

    //Set the name and joint anim values array of the new animation clip
    fakeClip.m_name = name;
    fakeClip.m_jointAnimValues = currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues;

    //Init default values for each joint
    if (jointCount > 0) {
        fakeClip.m_jointAnimValues[0].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 2.0f };
        fakeClip.m_jointAnimValues[0].endPos = DirectX::XMFLOAT3{ 0.0f, 1.0f, 2.0f };
        fakeClip.m_jointAnimValues[0].startRot = DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[0].endRot = DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[0].startAngle = 5.0f;
        fakeClip.m_jointAnimValues[0].endAngle = 160.0f;
    }
    if (jointCount > 1) {
        fakeClip.m_jointAnimValues[1].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 4.0f };
        fakeClip.m_jointAnimValues[1].endPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 3.5f };
        fakeClip.m_jointAnimValues[1].startRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[1].endRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[1].startAngle = -17.0f;
        fakeClip.m_jointAnimValues[1].endAngle = 58.0f;
    }
    if (jointCount > 2) {
        fakeClip.m_jointAnimValues[2].startPos = DirectX::XMFLOAT3{ 1.5f, -2.0f, -6.0f };
        fakeClip.m_jointAnimValues[2].endPos = DirectX::XMFLOAT3{ 1.5f, -2.0f, -6.0f };
        fakeClip.m_jointAnimValues[2].startRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[2].endRot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
        fakeClip.m_jointAnimValues[2].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[2].endAngle = DirectX::XM_PI;
    }

    Animation newAnim;
    newAnim.m_name = name;
    newAnim.m_jointAnimValues = fakeClip.m_jointAnimValues;

    int currentJointCount = newAnim.m_jointAnimValues.size();
    for (int i = 0; i < currentJointCount; i++)
    {
        auto& currentJointValues = newAnim.m_jointAnimValues[i];

        CreateAnimationSampler(currentObject, i, &newAnim,
            currentJointValues.startPos, currentJointValues.endPos,
            currentJointValues.startRot, currentJointValues.endRot,
            currentJointValues.startAngle, currentJointValues.endAngle);
    }

    //Resizing here since the default values above creates 
    currentObject.m_robotArmAnimations.resize(currentAnimClipIndex);

    return newAnim;
}

Animation Scene::CreateRobotArmAnimationSpin(SceneObject& currentObject, std::string name)
{
    //Create animation clip instance to which values will be added
    Animation fakeClip;

    //Make sure robot animations array has space for new clip
    int prevAnimClipSize = currentObject.m_robotArmAnimations.size();
    currentObject.m_robotArmAnimations.resize(prevAnimClipSize + 1);
    int currentAnimClipIndex = currentObject.m_robotArmAnimations.size() - 1;

    //Make sure joint anim values array of the current animation clip has space for all the joints
    int jointCount = currentObject.m_robotArmSkeleton.GetBoneCount();
    currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues.resize(jointCount);

    //Set the name and joint anim values array of the new animation clip
    fakeClip.m_name = name;
    fakeClip.m_jointAnimValues = currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues;

    //Init default values for each joint
    if (jointCount > 0) {
        fakeClip.m_jointAnimValues[0].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 2.0f };
        fakeClip.m_jointAnimValues[0].endPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 2.0f };
        fakeClip.m_jointAnimValues[0].startRot = DirectX::XMFLOAT3{ 0.0f,1.0f, 0.0f };
        fakeClip.m_jointAnimValues[0].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[0].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[0].endAngle = DirectX::XM_PI;
    }
    if (jointCount > 1) {
        fakeClip.m_jointAnimValues[1].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].endPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].startRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[1].endAngle = DirectX::XM_PI;
    }
    if (jointCount > 2) {
        fakeClip.m_jointAnimValues[2].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].endPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].startRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[2].endAngle = DirectX::XM_PI;
    }

    Animation newAnim;
    newAnim.m_name = name;
    newAnim.m_jointAnimValues = fakeClip.m_jointAnimValues;

    int currentJointCount = newAnim.m_jointAnimValues.size();
    for (int i = 0; i < currentJointCount; i++)
    {
        auto& currentJointValues = newAnim.m_jointAnimValues[i];

        CreateAnimationSampler(currentObject, i, &newAnim,
            currentJointValues.startPos, currentJointValues.endPos,
            currentJointValues.startRot, currentJointValues.endRot,
            currentJointValues.startAngle, currentJointValues.endAngle);
    }

    //Resizing here since the default values above creates an extra empty clip
    currentObject.m_robotArmAnimations.resize(currentAnimClipIndex);

    return newAnim;
}

Animation Scene::CreateRobotArmAnimationDingDong(SceneObject& currentObject, std::string name)
{
    //Create animation clip instance to which values will be added
    Animation fakeClip;

    //Make sure robot animations array has space for new clip
    int prevAnimClipSize = currentObject.m_robotArmAnimations.size();
    currentObject.m_robotArmAnimations.resize(prevAnimClipSize + 1);
    int currentAnimClipIndex = currentObject.m_robotArmAnimations.size() - 1;

    //Make sure joint anim values array of the current animation clip has space for all the joints
    int jointCount = currentObject.m_robotArmSkeleton.GetBoneCount();
    currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues.resize(jointCount);

    //Set the name and joint anim values array of the new animation clip
    fakeClip.m_name = name;
    fakeClip.m_jointAnimValues = currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues;

    //Init default values for each joint
    if (jointCount > 0) {
        fakeClip.m_jointAnimValues[0].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 6.0f };
        fakeClip.m_jointAnimValues[0].endPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 6.0f };
        fakeClip.m_jointAnimValues[0].startRot = DirectX::XMFLOAT3{ -10.0f, -1.0f, 0.0f };
        fakeClip.m_jointAnimValues[0].endRot = DirectX::XMFLOAT3{ -10.0f, -1.0f, 0.0f };
        fakeClip.m_jointAnimValues[0].startAngle = 360.0f;
        fakeClip.m_jointAnimValues[0].endAngle = 180.0f;
    }
    if (jointCount > 1) {
        fakeClip.m_jointAnimValues[1].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].endPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].startRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[1].endAngle = DirectX::XM_PI;
    }
    if (jointCount > 2) {
        fakeClip.m_jointAnimValues[2].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].endPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].startRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[2].endAngle = DirectX::XM_PI;
    }

    Animation newAnim;
    newAnim.m_name = name;
    newAnim.m_jointAnimValues = fakeClip.m_jointAnimValues;

    int currentJointCount = newAnim.m_jointAnimValues.size();
    for (int i = 0; i < currentJointCount; i++)
    {
        auto& currentJointValues = newAnim.m_jointAnimValues[i];

        CreateAnimationSampler(currentObject, i, &newAnim,
            currentJointValues.startPos, currentJointValues.endPos,
            currentJointValues.startRot, currentJointValues.endRot,
            currentJointValues.startAngle, currentJointValues.endAngle);
    }

    //Resizing here since the default values above creates an extra empty clip
    currentObject.m_robotArmAnimations.resize(currentAnimClipIndex);

    return newAnim;
}

Animation Scene::CreateRobotArmAnimationCluster(SceneObject& currentObject, std::string name)
{
    //Create animation clip instance to which values will be added
    Animation fakeClip;

    //Make sure robot animations array has space for new clip
    int prevAnimClipSize = currentObject.m_robotArmAnimations.size();
    currentObject.m_robotArmAnimations.resize(prevAnimClipSize + 1);
    int currentAnimClipIndex = currentObject.m_robotArmAnimations.size() - 1;

    //Make sure joint anim values array of the current animation clip has space for all the joints
    int jointCount = currentObject.m_robotArmSkeleton.GetBoneCount();
    currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues.resize(jointCount);

    //Set the name and joint anim values array of the new animation clip
    fakeClip.m_name = name;
    fakeClip.m_jointAnimValues = currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues;

    //Init default values for each joint
    if (jointCount > 0) {
        fakeClip.m_jointAnimValues[0].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 5.0f };
        fakeClip.m_jointAnimValues[0].endPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 5.0f };
        fakeClip.m_jointAnimValues[0].startRot = DirectX::XMFLOAT3{ 0.0f,1.0f, 0.0f };
        fakeClip.m_jointAnimValues[0].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[0].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[0].endAngle = DirectX::XM_PIDIV4;
    }
    if (jointCount > 1) {
        fakeClip.m_jointAnimValues[1].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].endPos = DirectX::XMFLOAT3{ 0.0f, -2.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].startRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[1].endAngle = DirectX::XM_PI;
    }
    if (jointCount > 2) {
        fakeClip.m_jointAnimValues[2].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].endPos = DirectX::XMFLOAT3{ 0.0f, -2.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].startRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[2].endAngle = DirectX::XM_PI;
    }

    Animation newAnim;
    newAnim.m_name = name;
    newAnim.m_jointAnimValues = fakeClip.m_jointAnimValues;

    int currentJointCount = newAnim.m_jointAnimValues.size();
    for (int i = 0; i < currentJointCount; i++)
    {
        auto& currentJointValues = newAnim.m_jointAnimValues[i];

        CreateAnimationSampler(currentObject, i, &newAnim,
            currentJointValues.startPos, currentJointValues.endPos,
            currentJointValues.startRot, currentJointValues.endRot,
            currentJointValues.startAngle, currentJointValues.endAngle);
    }

    //Resizing here since the default values above creates an extra empty clip
    currentObject.m_robotArmAnimations.resize(currentAnimClipIndex);

    return newAnim;
}

Animation Scene::CreateRobotArmAnimationLaunch(SceneObject& currentObject, std::string name)
{
    //Create animation clip instance to which values will be added
    Animation fakeClip;

    //Make sure robot animations array has space for new clip
    int prevAnimClipSize = currentObject.m_robotArmAnimations.size();
    currentObject.m_robotArmAnimations.resize(prevAnimClipSize + 1);
    int currentAnimClipIndex = currentObject.m_robotArmAnimations.size() - 1;

    //Make sure joint anim values array of the current animation clip has space for all the joints
    int jointCount = currentObject.m_robotArmSkeleton.GetBoneCount();
    currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues.resize(jointCount);

    //Set the name and joint anim values array of the new animation clip
    fakeClip.m_name = name;
    fakeClip.m_jointAnimValues = currentObject.m_robotArmAnimations[currentAnimClipIndex].m_jointAnimValues;

    //Init default values for each joint
    if (jointCount > 0) {
        fakeClip.m_jointAnimValues[0].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 6.0f };
        fakeClip.m_jointAnimValues[0].endPos = DirectX::XMFLOAT3{ -30.0f, -2.0f, 7.0f };
        fakeClip.m_jointAnimValues[0].startRot = DirectX::XMFLOAT3{ 1.0f,1.0f, 0.0f };
        fakeClip.m_jointAnimValues[0].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 1.0f };
        fakeClip.m_jointAnimValues[0].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[0].endAngle = DirectX::XM_PI;
    }
    if (jointCount > 1) {
        fakeClip.m_jointAnimValues[1].startPos = DirectX::XMFLOAT3{ 0.0f, 2.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].endPos = DirectX::XMFLOAT3{ -15.0f, 1.0f, 10.0f };
        fakeClip.m_jointAnimValues[1].startRot = DirectX::XMFLOAT3{ -1.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[1].endRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 1.0f };
        fakeClip.m_jointAnimValues[1].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[1].endAngle = DirectX::XM_PI;
    }
    if (jointCount > 2) {
        fakeClip.m_jointAnimValues[2].startPos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].endPos = DirectX::XMFLOAT3{ -10.0f, -5.0f, 8.0f };
        fakeClip.m_jointAnimValues[2].startRot = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        fakeClip.m_jointAnimValues[2].endRot = DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f };
        fakeClip.m_jointAnimValues[2].startAngle = 0.0f;
        fakeClip.m_jointAnimValues[2].endAngle = DirectX::XM_PI;
    }

    Animation newAnim;
    newAnim.m_name = name;
    newAnim.m_jointAnimValues = fakeClip.m_jointAnimValues;

    int currentJointCount = newAnim.m_jointAnimValues.size();
    for (int i = 0; i < currentJointCount; i++)
    {
        auto& currentJointValues = newAnim.m_jointAnimValues[i];

        CreateAnimationSampler(currentObject, i, &newAnim,
            currentJointValues.startPos, currentJointValues.endPos,
            currentJointValues.startRot, currentJointValues.endRot,
            currentJointValues.startAngle, currentJointValues.endAngle);
    }

    //Resizing here since the default values above creates an extra empty clip
    currentObject.m_robotArmAnimations.resize(currentAnimClipIndex);

    return newAnim;
}

Animation Scene::CreateRobotArmAnimationFromExisting(SceneObject& currentObject, int index)
{
    Animation newAnim;
    newAnim.m_name = currentObject.m_robotArmAnimations[index].m_name;
    newAnim.m_jointAnimValues = currentObject.m_robotArmAnimations[index].m_jointAnimValues;
    newAnim.m_samplers.clear();
    newAnim.m_channels.clear();

    int jointCount = newAnim.m_jointAnimValues.size();
    for (int i = 0; i < jointCount; i++)
    {
        auto& currentJointValues = currentObject.m_robotArmAnimations[index].m_jointAnimValues[i];

        CreateAnimationSampler(currentObject, i, &newAnim,
            currentJointValues.startPos, currentJointValues.endPos,
            currentJointValues.startRot, currentJointValues.endRot,
            currentJointValues.startAngle, currentJointValues.endAngle);
    }

    return newAnim;
}

void Scene::CreateAnimationSampler(SceneObject& currentObject, int nodeIndex, Animation* anim, const DirectX::XMFLOAT3& startP, const DirectX::XMFLOAT3& endP, const DirectX::XMFLOAT3& startR, const DirectX::XMFLOAT3& endR, float startAngle, float endAngle)
{
    // Samplers for the hand's translation and rotation.
    AnimationSampler nodeTranslationSampler, nodeRotationSampler;

    // Get the hand's structural bind pose.
    DirectX::XMMATRIX nodeBindPose = DirectX::XMLoadFloat4x4(&currentObject.m_robotArmSkeleton.GetJoint(nodeIndex)->localBindTransform);

    // --- Keyframe 1: The Start Pose (t = 0.0s) ---
    // The hand is in its default, non-animated state.
    XMFLOAT3 startPos = BakeTranslationOntoBindPose(nodeBindPose, startP);
    nodeTranslationSampler.timestamps.push_back(0.0f);
    nodeTranslationSampler.vec3_values.push_back(startPos);

    XMFLOAT4 startRot = BakeRotationOntoBindPose(nodeBindPose, startR, startAngle); // No rotation
    nodeRotationSampler.timestamps.push_back(0.0f);
    nodeRotationSampler.vec4_values.push_back(startRot);

    // --- Keyframe 2: The End Pose (t = 2.0s) ---
    // The hand is translated up and rotated 90 degrees to the side.
    XMFLOAT3 endPos = BakeTranslationOntoBindPose(nodeBindPose, endP); // Move up slightly
    nodeTranslationSampler.timestamps.push_back(4.0f);
    nodeTranslationSampler.vec3_values.push_back(endPos);

    XMFLOAT4 endRot = BakeRotationOntoBindPose(nodeBindPose, endR, endAngle); // Rotate 90 degrees
    nodeRotationSampler.timestamps.push_back(4.0f);
    nodeRotationSampler.vec4_values.push_back(endRot);

    // --- Add Samplers and Channels for the node ---
    anim->m_samplers.push_back(nodeTranslationSampler); // Sampler x
    int nodeTranslationSamplerIndex = anim->m_samplers.size() - 1;
    anim->m_samplers.push_back(nodeRotationSampler);    // Sampler x+1
    int nodeRotationSamplerIndex = anim->m_samplers.size() - 1;

    AnimationChannel transChannel;
    transChannel.path = AnimationChannel::TRANSLATION;
    transChannel.samplerIndex = nodeTranslationSamplerIndex;
    transChannel.jointIndex = nodeIndex;
    anim->m_channels.push_back(transChannel);

    AnimationChannel rotChannel;
    rotChannel.path = AnimationChannel::ROTATION;
    rotChannel.samplerIndex = nodeRotationSamplerIndex;
    rotChannel.jointIndex = nodeIndex;
    anim->m_channels.push_back(rotChannel);
}

//-------------------- ===[[[[WEEK 7.2 Pole & Fox Definitions here]]]]=== --------------------
void Scene::SkeletonPoleInit() 
{
    bool ok = m_sceneobjectSkinning.LoadGLTFWithSkeleton(m_ctx, L"Resources\\simplerig.gltf");
	Skeleton* skeleton = m_sceneobjectSkinning.GetRootNode(0)->GetSkeleton();
	skeleton->AddAnimation(CreatePoleAnimation(skeleton));
}

Animation* Scene::CreatePoleAnimation(Skeleton* skeleton)
{
    Animation* animNode = new Animation();
    animNode->m_name = "WavePole_PreSkin";
    CreateWaveAnimationSamplerForPole(1 , animNode, skeleton);

    return animNode;
}

void Scene::CreateWaveAnimationSamplerForPole(int nodeIndex, Animation* anim, Skeleton* skeleton)
{
    // Samplers for the hand's translation and rotation.
    AnimationSampler nodeTranslationSampler, nodeRotationSampler;

    // Get the hand's structural bind pose.
    DirectX::XMMATRIX nodeBindPose = DirectX::XMLoadFloat4x4(&skeleton->GetJoint(nodeIndex)->localBindTransform);

    // --- Keyframe 1: The Start Pose (t = 0.0s) ---
    // The hand is in its default, non-animated state.
    XMFLOAT3 startPos = BakeTranslationOntoBindPose(nodeBindPose, { 0.0f, 0.0f, 0.0f });
    nodeTranslationSampler.timestamps.push_back(0.0f);
    nodeTranslationSampler.vec3_values.push_back(startPos);

    XMFLOAT4 startRot = BakeRotationOntoBindPose(nodeBindPose, { 0, 1, 0 }, 0.0f); // No rotation
    nodeRotationSampler.timestamps.push_back(0.0f);
    nodeRotationSampler.vec4_values.push_back(startRot);

    // --- Keyframe 2: The End Pose (t = 2.0s) ---
    // The hand is translated up and rotated 90 degrees to the side.
    XMFLOAT3 endPos = BakeTranslationOntoBindPose(nodeBindPose, { 0.0f, 0.0f, 0.0f }); 
    nodeTranslationSampler.timestamps.push_back(2.0f);
    nodeTranslationSampler.vec3_values.push_back(endPos);

    XMFLOAT4 endRot = BakeRotationOntoBindPose(nodeBindPose, { 0, 1, 0 }, DirectX::XM_PIDIV4); //45 degrees
    nodeRotationSampler.timestamps.push_back(2.0f);
    nodeRotationSampler.vec4_values.push_back(endRot);

    // --- Add Samplers and Channels for the node ---
    anim->m_samplers.push_back(nodeTranslationSampler); // Sampler x
    int nodeTranslationSamplerIndex = anim->m_samplers.size() - 1;
    anim->m_samplers.push_back(nodeRotationSampler);    // Sampler x+1
    int nodeRotationSamplerIndex = anim->m_samplers.size() - 1;

    AnimationChannel transChannel;
    transChannel.path = AnimationChannel::TRANSLATION;
    transChannel.samplerIndex = nodeTranslationSamplerIndex;
    transChannel.jointIndex = nodeIndex;
    anim->m_channels.push_back(transChannel);

    AnimationChannel rotChannel;
    rotChannel.path = AnimationChannel::ROTATION;
    rotChannel.samplerIndex = nodeRotationSamplerIndex;
    rotChannel.jointIndex = nodeIndex;
    anim->m_channels.push_back(rotChannel);
}

void Scene::FoxInit() 
{
    bool ok = m_sceneobjectFox.LoadGLTFWithSkeleton(m_ctx, L"Resources\\Fox.gltf");
    m_sceneobjectFox.AddTranslationToRoots({0, -0.5 ,4});
}

void Scene::update(const float deltaTime)
{
    // note the pixel shader and the vertex shader have been set by the renderer class calling this method
    m_pImmediateContext->PSSetShaderResources(0, 1, &m_pTextureDiffuse);
    m_pImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

    ConstantBuffer cb1;
    cb1.mView = XMMatrixTranspose(getCamera()->getViewMatrix());
    cb1.mProjection = XMMatrixTranspose(getCamera()->getProjectionMatrix());
    cb1.vOutputColor = XMFLOAT4(0, 0, 0, 0);

    m_lightProperties.EyePosition = XMFLOAT4(m_pCamera->getPosition().x, m_pCamera->getPosition().y, m_pCamera->getPosition().z, 1);
    m_pImmediateContext->UpdateSubresource(m_pLightConstantBuffer.Get(), 0, nullptr, &m_lightProperties, 0, 0);
    ID3D11Buffer* buf = m_pLightConstantBuffer.Get();
    m_pImmediateContext->PSSetConstantBuffers(1, 1, &buf);

    //-------------------------------------|||[[[[[ ANIMATION WEEK 6, 7 LOGIC HERE]]]]]|||-------------------------------------
#pragma region Week6
    //animationTimer += deltaTime;
    ////Loop through channels and their samplers, then their samplers timestamps to set the animation
    //for (int i = 0; i < m_AnimationClass.m_channels.size(); i++)
    //{
    //    AnimationChannel currentChannel = m_AnimationClass.m_channels[i];
    //    AnimationSampler currentSampler = m_AnimationClass.m_samplers[currentChannel.samplerIndex];
    //    SceneNode* currentNode = nullptr;

    //    if (currentChannel.jointIndex == 0)
    //    {
    //        currentNode = m_sceneobject.GetRootNode(0);
    //    }
    //    else
    //    {
    //        currentNode = m_sceneobject.GetRootNode(0)->GetChildNode(currentChannel.jointIndex - 1);
    //    }

    //    int nextKeyFrame = -1;
    //    for (int j = 0; j < currentSampler.timestamps.size(); j++)
    //    {
    //        if (currentSampler.timestamps[j] > animationTimer)
    //        {
    //            nextKeyFrame = j;
    //            break;
    //        }
    //    }

    //    if (nextKeyFrame == -1 || nextKeyFrame == 0) nextKeyFrame = 1; //Keyframe to play, based on timestamp and t value(progress variable)
    //    int prevKeyFrame = nextKeyFrame - 1;
    //    float prevTime = currentSampler.timestamps[prevKeyFrame];
    //    float nextTime = currentSampler.timestamps[nextKeyFrame];
    //    float t = (animationTimer - prevTime) / (nextTime - prevTime);
    //    if (animationTimer >= nextTime) animationTimer = 0; //Reset or Loop the animation if its finished

    //    if (m_AnimationClass.m_channels[i].path == AnimationChannel::TRANSLATION)
    //    {
    //        DirectX::XMVECTOR prevValue = DirectX::XMLoadFloat3(&currentSampler.vec3_values[prevKeyFrame]);
    //        DirectX::XMVECTOR nextValue = DirectX::XMLoadFloat3(&currentSampler.vec3_values[nextKeyFrame]);
    //        DirectX::XMVECTOR currentTrans = DirectX::XMVectorLerp(prevValue, nextValue, t);
    //        translationMatrix = DirectX::XMMatrixTranslationFromVector(currentTrans);
    //    }
    //    else if (m_AnimationClass.m_channels[i].path == AnimationChannel::ROTATION)
    //    {
    //        DirectX::XMVECTOR prevValue = DirectX::XMLoadFloat4(&currentSampler.vec4_values[prevKeyFrame]);
    //        DirectX::XMVECTOR nextValue = DirectX::XMLoadFloat4(&currentSampler.vec4_values[nextKeyFrame]);
    //        DirectX::XMVECTOR currentRot = DirectX::XMQuaternionSlerp(prevValue, nextValue, t);
    //        rotationMatrix = DirectX::XMMatrixRotationQuaternion(currentRot);
    //    }
    //    else if (m_AnimationClass.m_channels[i].path == AnimationChannel::SCALE)
    //    {
    //        //DirectX::XMVECTOR prevValue = DirectX::XMLoadFloat3(&currentSampler.vec3_values[prevKeyFrame]);
    //        //DirectX::XMVECTOR nextValue = DirectX::XMLoadFloat3(&currentSampler.vec3_values[nextKeyFrame]);
    //        //DirectX::XMVECTOR currentScale = DirectX::XMVectorLerp(prevValue, nextValue, t);
    //        //scaleMatrix = DirectX::XMMatrixTranslationFromVector(currentScale);
    //    }

    //    currentNode->SetMatrix(rotationMatrix * translationMatrix);
    //}
#pragma endregion
#pragma region Week7.1 Robot Arm Animations Here
    //1. Select and Play Animation: (Here you could use keyboard input or ImGui to choose an animation from your vector)
    static bool doOnce = true;
    if (doOnce)
    {
        doOnce = false;
        for (auto& obj : m_hierarchicalObjects) 
        {
            if (!obj.m_robotArmAnimations.empty())
            {
                obj.m_robotArmSkeleton.PlayAnimation(&obj.m_robotArmAnimations[0]);
            }
        }

    }

    //2. Update the Skeleton's Pose: This function now reads from the Animation object and updates all joint poses.
    for (auto& obj : m_hierarchicalObjects)
    {
        obj.m_robotArmSkeleton.Update(deltaTime);


        //3. Sync Visible Nodes with Skeleton (This logic is crucial): This copies the final world matrix of each joint to its visible sphere.
        for (int i = 0; i < obj.m_robotArmSkeleton.GetBoneCount(); ++i)
        {
            Joint* joint = obj.m_robotArmSkeleton.GetJoint(i);
            DirectX::XMMATRIX finalWorldTransform = XMLoadFloat4x4(&joint->finalTransform);

            obj.m_armSegmentNodes[i]->SetMatrix(finalWorldTransform);
        }
    }

   /* Note that m_armSegmentNodes is linked to m_sceneobject so you just need to call the 
    m_sceneobject AnimateFrame and RenderFrame methods as you may already be doing */
#pragma endregion
#pragma region Week7.2 Pole & Fox Animations Here
    //Pole Rig
    Skeleton* s = m_sceneobjectSkinning.GetRootNode(0)->GetSkeleton();
    static bool doOnce2 = true;
    if (doOnce2)
    {
        doOnce2 = false;
        if (s->GetAnimationCount() > 0)
        {
            s->PlayAnimation((unsigned int)1);
        }
    }

    //Fox Rig
    static bool doOnce3 = true;
    if (doOnce3) 
    {
        doOnce3 = false;
        m_sceneobjectFox.GetRootNode(0)->GetSkeleton()->PlayAnimation(2);
    }
#pragma endregion

    m_sceneobject.AnimateFrame(m_ctx); // this updates the transform matrix for the object - this should be called after all transforms have been made 
    m_sceneobject.RenderFrame(m_ctx, deltaTime); // renders the object
    
    for(SceneObject& obj : m_hierarchicalObjects)
    {
        obj.m_HierarchicalObject.AnimateFrame(m_ctx);
        obj.m_HierarchicalObject.RenderFrame(m_ctx, deltaTime);
	}

    m_sceneobjectSkinning.AnimateFrame(m_ctx);
    m_sceneobjectSkinning.RenderFrame(m_ctx, deltaTime);
    m_sceneobjectFox.AnimateFrame(m_ctx);
    m_sceneobjectFox.RenderFrame(m_ctx, deltaTime);
}
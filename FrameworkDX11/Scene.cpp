#include "Scene.h"
#include "DDSTextureLoader.h"


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
    
    //-------------------- ANIMATION HIERARCHY INIT HERE --------------------
    if (bool animInit = AnimationHierarchyInit() == E_FAIL) return E_FAIL;

    m_pCamera = new Camera(XMFLOAT3(0, 0, -6), XMFLOAT3(0, 0, 1), XMFLOAT3(0.0f, 1.0f, 0.0f), width, height);
    
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

HRESULT Scene::AnimationHierarchyInit()
{
    //---------------------|||[[[[[ HIERARCHY CREATION START ]]]]]|||---------------------
    //PARENT NODE INITIALIZATION
    SceneNode* sunNode = m_sceneobject.CreateRootNode();
    if (!sunNode) return E_FAIL;
    sunNode->LoadSphere(m_ctx);
    sunNode->AddScale(1.5);
    sunNode->AddTranslation({ 0.0, 0, 2.0 });
    //Parent node: translation logic
    AnimationChannel channel0translation;
    AnimationSampler sampler0Translation;
    sampler0Translation.vec3_values.push_back(m_startPosParent);// Setting position keyframes
    sampler0Translation.vec3_values.push_back(m_endPosParent);
    sampler0Translation.timestamps.push_back(0.0f);// Setting time keyframes for each keyframe
    sampler0Translation.timestamps.push_back(2.0f);
    m_AnimationClass.m_samplers.push_back(sampler0Translation);
    m_AnimationClass.m_channels.push_back({ channel0translation.TRANSLATION, 0, 0 });
    //Parent node: rotation logic
    AnimationChannel channel1Rotation;
    AnimationSampler sampler1Rotation;
    DirectX::XMStoreFloat4(&m_startRotParent, DirectX::XMQuaternionIdentity());
    DirectX::XMStoreFloat4(&m_endRotParent, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 0.85, 0, 0.52), DirectX::XM_PI));
    sampler1Rotation.vec4_values.push_back(m_startRotParent);//Rotation keyframes
    sampler1Rotation.vec4_values.push_back(m_endRotParent);
    sampler1Rotation.timestamps.push_back(0.0f); // Rotation timestamps of each keyframe
    sampler1Rotation.timestamps.push_back(2.0f);
    m_AnimationClass.m_samplers.push_back(sampler1Rotation);
    m_AnimationClass.m_channels.push_back({ channel1Rotation.ROTATION, 0, 1 });

#pragma region child 1
    //CHILD NODE INITIALIZATION
    SceneNode* planetOneNode = sunNode->CreateChildNode();
    if (!planetOneNode) return E_FAIL;
    if (!planetOneNode->LoadSphere(m_ctx));
    planetOneNode->AddTranslation({ 0, 2.5, 0 });
    planetOneNode->AddScale(0.75);
    //Child one node: translation logic
    AnimationChannel channel2translation;
    AnimationSampler sampler2Translation;
    sampler2Translation.vec3_values.push_back(m_startPosChild1);
    sampler2Translation.vec3_values.push_back({ m_endPosChild1 });
    sampler2Translation.timestamps.push_back(0.0f);
    sampler2Translation.timestamps.push_back(2.0f);
    m_AnimationClass.m_samplers.push_back(sampler2Translation);
    m_AnimationClass.m_channels.push_back({ channel2translation.TRANSLATION, 1, 2 });
    //Child one node: rotation logic
    AnimationChannel channel3Rotation;
    AnimationSampler sampler3Rotation;
    sampler3Rotation.vec4_values.push_back({ m_startRotChild1 });
    sampler3Rotation.vec4_values.push_back({ m_endRotChild1 });
    sampler3Rotation.timestamps.push_back(0.0f);
    sampler3Rotation.timestamps.push_back(2.0f);
    m_AnimationClass.m_samplers.push_back(sampler3Rotation);
    m_AnimationClass.m_channels.push_back({ channel3Rotation.ROTATION, 1, 3 });
#pragma endregion

#pragma region child 2
    ////CHILD 2 NODE INITIALIZATION
    SceneNode* planetTwoNode = sunNode->CreateChildNode();
    if (!planetTwoNode) return E_FAIL;
    if (!planetTwoNode->LoadSphere(m_ctx));
    planetTwoNode->AddTranslation({ 0, 2.5, 0 });
    planetTwoNode->AddScale(0.75);
    //Child two node: translation logic
    AnimationChannel channel4translation;
    AnimationSampler sampler4Translation;
    sampler4Translation.vec3_values.push_back({0.f, 0.f, 0.f });
    sampler4Translation.vec3_values.push_back({ 5.f, 0.f, 0.f });
    sampler4Translation.timestamps.push_back(0.0f);
    sampler4Translation.timestamps.push_back(2.0f);
    m_AnimationClass.m_samplers.push_back(sampler4Translation);
    m_AnimationClass.m_channels.push_back({ channel4translation.TRANSLATION, 2, 4 });
    //Child two node: rotation logic
    AnimationChannel channel5otation;
    AnimationSampler sampler5Rotation;
    sampler5Rotation.vec4_values.push_back({ m_startRotChild1 });
    sampler5Rotation.vec4_values.push_back({ m_endRotChild1 });
    sampler5Rotation.timestamps.push_back(0.0f);
    sampler5Rotation.timestamps.push_back(2.0f);
    m_AnimationClass.m_samplers.push_back(sampler5Rotation);
    m_AnimationClass.m_channels.push_back({ channel5otation.ROTATION, 2, 5 });
#pragma endregion
    //---------------------|||[[[[[ HIERARCHY CREATION END ]]]]]|||---------------------
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


    //-----------------------------------------|||[[[[[ ANIMATION UPDATE LOGIC HERE]]]]]|||-----------------------------------------
    animationTimer += deltaTime;
    //Loop through channels and their samplers, then their samplers timestamps to set the animation
    for (int i = 0; i < m_AnimationClass.m_channels.size(); i++) 
    {
        AnimationChannel currentChannel = m_AnimationClass.m_channels[i];
		AnimationSampler currentSampler = m_AnimationClass.m_samplers[currentChannel.samplerIndex];
        SceneNode* currentNode = nullptr;

        if (currentChannel.jointIndex == 0) 
        {
            currentNode = m_sceneobject.GetRootNode(0);
        } 
        else
        {
            currentNode = m_sceneobject.GetRootNode(0)->GetChildNode(currentChannel.jointIndex - 1);
        }

        int nextKeyFrame = -1;
        for (int j = 0; j < currentSampler.timestamps.size(); j++)
        {
            if (currentSampler.timestamps[j] > animationTimer)
            {
                nextKeyFrame = j;
                break;
            }
        }

        if (nextKeyFrame == -1 || nextKeyFrame == 0) nextKeyFrame = 1; //Keyframe to play, based on timestamp and t value(progress variable)
        int prevKeyFrame = nextKeyFrame - 1;
        float prevTime = currentSampler.timestamps[prevKeyFrame];
        float nextTime = currentSampler.timestamps[nextKeyFrame];
        float t = (animationTimer - prevTime) / (nextTime - prevTime);
        if (animationTimer >= nextTime) animationTimer = 0; //Reset or Loop the animation if its finished

        DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixIdentity();

        if (m_AnimationClass.m_channels[i].path == AnimationChannel::TRANSLATION)
        {
            DirectX::XMVECTOR prevValue = DirectX::XMLoadFloat3(&currentSampler.vec3_values[prevKeyFrame]);
            DirectX::XMVECTOR nextValue = DirectX::XMLoadFloat3(&currentSampler.vec3_values[nextKeyFrame]);
            DirectX::XMVECTOR currentTrans = DirectX::XMVectorLerp(prevValue, nextValue, t);
            translationMatrix = DirectX::XMMatrixTranslationFromVector(currentTrans);
            currentNode->SetMatrix(translationMatrix);   
        }
        else if (m_AnimationClass.m_channels[i].path == AnimationChannel::ROTATION)
        {
            //DirectX::XMVECTOR prevValue = DirectX::XMLoadFloat4(&currentSampler.vec4_values[prevKeyFrame]);
            //DirectX::XMVECTOR nextValue = DirectX::XMLoadFloat4(&currentSampler.vec4_values[nextKeyFrame]);
            //DirectX::XMVECTOR currentRot = DirectX::XMQuaternionSlerp(prevValue, nextValue, t);
            //rotationMatrix = DirectX::XMMatrixTranslationFromVector(currentRot);
            //currentNode->SetMatrix(rotationMatrix * translationMatrix);
        }
        else if (m_AnimationClass.m_channels[i].path == AnimationChannel::SCALE) 
        {
            //DirectX::XMVECTOR prevValue = DirectX::XMLoadFloat3(&currentSampler.vec3_values[prevKeyFrame]);
            //DirectX::XMVECTOR nextValue = DirectX::XMLoadFloat3(&currentSampler.vec3_values[nextKeyFrame]);
            //DirectX::XMVECTOR currentScale = DirectX::XMVectorLerp(prevValue, nextValue, t);
            //scaleMatrix = DirectX::XMMatrixTranslationFromVector(currentScale);
            //currentNode->SetMatrix(scaleMatrix);
        }

        //currentNode->SetMatrix(scaleMatrix * rotationMatrix * translationMatrix);
    }

    m_sceneobject.AnimateFrame(m_ctx); // this updates the transform matrix for the object - this should be called after all transforms have been made 
    m_sceneobject.RenderFrame(m_ctx, deltaTime); // renders the object
}
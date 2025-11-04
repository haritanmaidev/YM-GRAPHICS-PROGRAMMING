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
	HRESULT hr2;

    m_ctx.Init(device.Get(), context.Get(), renderer);

    //Animation stuff
	bool animok3 = m_sceneobject2.LoadSphere(m_ctx);
    AnimationSampler animPosition;
	animPosition.vec3_values.push_back(m_startPos);
	animPosition.vec3_values.push_back(m_midPos);
	animPosition.vec3_values.push_back(m_endPos);
	animPosition.timestamps.push_back(0.0f);
	animPosition.timestamps.push_back(1.0f);
	animPosition.timestamps.push_back(2.0f);
	m_myAnimation.m_samplers.push_back(animPosition);
    DirectX:XMStoreFloat4(&m_startRot, DirectX::XMQuaternionIdentity());
    DirectX::XMStoreFloat4(&m_endRot, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XM_PI));
    //ALSO GIVE IT A NAME maybe
    
    //Setup Camera
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

    // Create the material constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBufferMaterials);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = m_pd3dDevice->CreateBuffer(&bd, nullptr, &m_pConstantBufferMaterials);
    if (FAILED(hr))
        return hr;

	//Setup light properties
    setupLightProperties();
    // Create the light constant buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(LightPropertiesConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = m_pd3dDevice->CreateBuffer(&bd, nullptr, &m_pLightConstantBuffer);
    if (FAILED(hr))
        return hr;

    // Load and setup textures
    hr = CreateDDSTextureFromFile(m_pd3dDevice.Get(), L"Resources\\rusty_metal_04_diff.dds", nullptr, &m_pTextureDiffuse);
	hr = CreateDDSTextureFromFile(m_pd3dDevice.Get(), L"Resources\\rusty_metal_04_rough.dds", nullptr, &m_pTextureRoughness);
	hr = CreateDDSTextureFromFile(m_pd3dDevice.Get(), L"Resources\\rusty_metal_04_metal.dds", nullptr, &m_pTextureMetallic);
	hr = CreateDDSTextureFromFile(m_pd3dDevice.Get(), L"Resources\\rusty_metal_04_ao.dds", nullptr, &m_pTextureAmbientOcclusion);
    // TextureCube setup
	hr = CreateDDSTextureFromFile(m_pd3dDevice.Get(), L"Resources\\SpecularCM.dds", nullptr, &m_pTextureSpecularIBL);
	hr = CreateDDSTextureFromFile(m_pd3dDevice.Get(), L"Resources\\DiffuseCM.dds", nullptr, &m_pTextureDiffuseIBL);
	hr = CreateDDSTextureFromFile(m_pd3dDevice.Get(), L"Resources\\AlbedoCM.dds", nullptr, &m_pTextureAlbedoIBL);
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
    //XMFLOAT4 LightPosition(m_pCamera->getPosition().x, m_pCamera->getPosition().y, m_pCamera->getPosition().z, 1);
    XMFLOAT4 LightPosition(5, 5,-6, 1);
    light.Position = LightPosition;
    
    m_lightProperties.EyePosition = LightPosition;
    m_lightProperties.Lights[0] = light;
}

void Scene::update(const float deltaTime)
{
    // note the pixel shader and the vertex shader have been set by the renderer class calling this method
    m_pImmediateContext->PSSetShaderResources(0, 1, &m_pTextureDiffuse);
    m_pImmediateContext->PSSetShaderResources(1, 1, &m_pTextureRoughness);
	m_pImmediateContext->PSSetShaderResources(2, 1, &m_pTextureMetallic);
	m_pImmediateContext->PSSetShaderResources(3, 1, &m_pTextureAmbientOcclusion);
	m_pImmediateContext->PSSetShaderResources(4, 1, &m_pTextureSpecularIBL);
	m_pImmediateContext->PSSetShaderResources(5, 1, &m_pTextureDiffuseIBL);
	m_pImmediateContext->PSSetShaderResources(6, 1, &m_pTextureAlbedoIBL);
    m_pImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinear);
	
    m_lightProperties.EyePosition = XMFLOAT4(m_pCamera->getPosition().x, m_pCamera->getPosition().y, m_pCamera->getPosition().z, 1);
    m_pImmediateContext->UpdateSubresource(m_pLightConstantBuffer.Get(), 0, nullptr, &m_lightProperties, 0, 0);
    ID3D11Buffer* buf = m_pLightConstantBuffer.Get();
    m_pImmediateContext->PSSetConstantBuffers(1, 1, &buf);

    //-------------------------------------[[[[[IMGUI Update]]]]]----------------------------------------
    m_pImmediateContext->UpdateSubresource(m_pConstantBufferMaterials.Get(), 0, nullptr, &m_materialProperties, 0, 0);
    ID3D11Buffer* matbuf = m_pConstantBufferMaterials.Get();
    m_pImmediateContext->PSSetConstantBuffers(2, 1, &matbuf);


    //----------------------------------------------=======================|||||[[[{{{ ANIMATION  STUFF }}}]]]}|||||=======================----------------------------------------------//
    //Sphere animation via sampler
    animationTimer += deltaTime;
    AnimationSampler sampler = m_myAnimation.m_samplers[0];
    int nextKeyframe = -1;

    for (int i = 0; i < sampler.timestamps.size(); i++) //Loop through sampler's timestamps to find next keyframe to animate
    {
        if (sampler.timestamps[i] > animationTimer)
        {
            nextKeyframe = i;
            break;
        }
    }

	if (nextKeyframe == -1 || nextKeyframe == 0) nextKeyframe = 1; // -1 = After last keyframe, 0 = before first keyframe

    int prevKeyframe = nextKeyframe - 1; //Sanity check: Calculate progress between these two timestamps
    float prevTime = sampler.timestamps[prevKeyframe];
    float nextTime = sampler.timestamps[nextKeyframe];
    float t = (animationTimer - prevTime) / (nextTime - prevTime);
    if (animationTimer >= nextTime) animationTimer = 0; //Resetting the animation once its finished

    DirectX::XMVECTOR prevValue = DirectX::XMLoadFloat3(&sampler.vec3_values[prevKeyframe]);
    DirectX::XMVECTOR nextValue = DirectX::XMLoadFloat3(&sampler.vec3_values[nextKeyframe]);
    DirectX::XMVECTOR finalValue = DirectX::XMVectorLerp(prevValue, nextValue, t);
    DirectX::XMMATRIX translationMatrix2 = DirectX::XMMatrixTranslationFromVector(finalValue);

    m_sceneobject2.GetRootNode(0)->SetMatrix(translationMatrix2);
    m_sceneobject2.AnimateFrame(m_ctx);
    m_sceneobject2.RenderFrame(m_ctx, deltaTime);
}

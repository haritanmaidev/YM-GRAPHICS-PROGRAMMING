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
    //bool ok = m_sceneobject.LoadSphere(m_ctx);
    //bool ok = m_sceneobject.LoadGLTF(m_ctx, L"Resources\\sphere.gltf");
	//bool ok2 = m_sceneobject2.LoadGLTF(m_ctx, L"Resources\\sphere.gltf");
    //bool ok = m_sceneobject.LoadGLTFWithSkeleton(m_ctx, L"Resources\\Fox.gltf");
    //bool ok = m_sceneobject.LoadGLTF(m_ctx, L"Resources\\FlightHelmet.gltf");
	bool animok = m_sceneobject3.LoadGLTF(m_ctx, L"Resources\\Box.gltf");   
	bool animok2 = m_scenePlanet.LoadGLTF(m_ctx, L"Resources\\Box.gltf");
    DirectX:XMStoreFloat4(&m_startRot, DirectX::XMQuaternionIdentity());    
    DirectX::XMStoreFloat4(&m_endRot, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XM_PI));


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

    m_pImmediateContext->UpdateSubresource(m_pConstantBufferMaterials.Get(), 0, nullptr, &m_materialProperties, 0, 0);
    ID3D11Buffer* matbuf = m_pConstantBufferMaterials.Get();
    m_pImmediateContext->PSSetConstantBuffers(2, 1, &matbuf);

    // scene object 1 
    m_sceneobject.AnimateFrame(m_ctx); // this updates the transform matrix for the object - this should be called after all transforms have been made
    m_sceneobject.RenderFrame(m_ctx, deltaTime); // renders the object

    // scene object 2
    XMMATRIX m = XMMatrixTranslation(4, 0, 0);
    m_sceneobject2.SetMatrixToRoots(m);
    m_sceneobject2.AnimateFrame(m_ctx); // this updates the transform matrix for the object - this should be called after all transforms have been made
    m_sceneobject2.RenderFrame(m_ctx, deltaTime); // renders the object

    //----------------------------------------------|||||||||||{[[[ ANIMATION  STUFF ]]]}|||||||||||----------------------------------------------//
    m_t += deltaTime * m_direction * 0.1;
    m_t2 += deltaTime;
    if (m_t > 1.0f || m_t < 0.0f) //Ping pong animation
    {
        m_direction *= -1.0f; // reverse direction
		m_t = max(0.0f, min(1.0f, m_t)); // clamps t between 0 and 1
    }

    //Parent object/sun setup
    DirectX::XMVECTOR currentPos = DirectX::XMVectorLerp
    (
        DirectX::XMLoadFloat3(&m_startPos), 
        DirectX::XMLoadFloat3(&m_endPos),
        m_t
    );

    DirectX::XMVECTOR currentRot = DirectX::XMQuaternionSlerp
    (
        DirectX::XMLoadFloat4(&m_startRot),
        DirectX::XMLoadFloat4(&m_endRot),
        m_t
	);

	//Compute final transformation matrix values
	DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslationFromVector(currentPos);
	DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(currentRot);
	DirectX::XMMATRIX finalMatrix = translationMatrix;
	//DirectX::XMMATRIX finalMatrix = DirectX::XMMatrixIdentity();   

    //Child object setup
    DirectX::XMMATRIX planetRotation = DirectX::XMMatrixRotationY(m_t2);
    DirectX::XMMATRIX planetTranslation = DirectX::XMMatrixTranslation(2.0f, 0.0f, 0.0f);
    DirectX::XMMATRIX planetLocal = planetTranslation * planetRotation; //NOTE: Order of multiplication matters, translation first, then rotation
    DirectX::XMMATRIX worldPlanet = planetLocal * finalMatrix;

    //Get objects's scene node and set its transform
	m_sceneobject3.GetRootNode(0)->SetMatrix(finalMatrix);
	m_scenePlanet.GetRootNode(0)->SetMatrix(worldPlanet);
    m_sceneobject3.AnimateFrame(m_ctx); // this updates the transform matrix for the object - this should be called after all transforms have been made
	m_scenePlanet.AnimateFrame(m_ctx);
    m_sceneobject3.RenderFrame(m_ctx, deltaTime); // renders the object
	m_scenePlanet.RenderFrame(m_ctx, deltaTime);
}

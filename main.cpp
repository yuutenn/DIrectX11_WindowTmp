#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "dxgi.lib")


#include <tchar.h>
#include <Windows.h>
#include <D3D11.h>
#include <D3DX11.h>
#include <DxErr.h>

//COM�|�C���^�̊J�������}�N��
#define SAFE_RELEASE(x) {if(x){(x)->Release(); (x)=nullptr;}}

#define WINDOW_W 640 //�E�B���h�E�̕�
#define WINDOW_H 480 //�E�B���h�E�̍���

HWND g_Hwnd; //�E�B���h�E

//�@�\���x��
D3D_FEATURE_LEVEL g_pFeatureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
UINT g_FeatureLevels = 3; //�z��̗v�f��
D3D_FEATURE_LEVEL g_FeatureLevelsSupported; //�f�o�C�X�쐬���ɕԂ����@�\���x��

//�C���^�[�t�F�C�X
ID3D11Device* g_pD3DDevice = nullptr; //�f�o�C�X
ID3D11DeviceContext* g_pImmediateContext = nullptr; //�f�o�C�X�E�R���e�L�X�g
IDXGISwapChain* g_pSwapChain = nullptr; //�X���b�v�`�F�C��

ID3D11RenderTargetView* g_pRenderTargetView = nullptr; //�`��^�[�Q�b�g�E�r���[
D3D11_VIEWPORT g_ViewPort[1]; //�r���[�|�[�g
ID3D11Texture2D* g_pDepthStencil; //�[�x/�X�e���V���E�e�N�X�`��
ID3D11DepthStencilView* g_pDepthStencilView = nullptr; //�[�x/�X�e���V���r���[

//�v���g�^�C�v�錾
void InitWindow(); //�E�B���h�E����
HRESULT InitD3D(); //D3D������
HRESULT InitBackBuffer(); //�o�b�N�o�b�t�@�̏�����
void Clear(); //��ʃN���A
HRESULT Flip(); //��ʕ\��
void CleanupD3D(); //D3D�㏈��
bool DeviceRemoveProc(); //�f�o�C�X��������


//�G���g���|�C���g
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	
	InitWindow();
	ShowWindow(g_Hwnd, SW_SHOWDEFAULT);
	UpdateWindow(g_Hwnd);

	if (FAILED(InitD3D())) {
		return 0;
	}

	MSG msg = {};
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message |= WM_QUIT){
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			//�f�o�C�X��������
			if (DeviceRemoveProc()) break;

			//�Q�[�����[�v
			Clear();
			Flip();
		}
	}

	//���
	CleanupD3D();
	return 0;
}

//�E�B���h�E�v���V�[�W��
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	
	HRESULT hr;
	BOOL fullscreen;
	
	//���b�Z�[�W����
	switch (uMsg) {
	case WM_DESTROY:
		//Direct3D�̏I������
		CleanupD3D();
		//�E�B���h�E�����
		PostQuitMessage(0);
		g_Hwnd = NULL;
		break;

	case WM_SIZE: //�T�C�Y�ύX����
		if (!g_pD3DDevice || wParam == SIZE_MINIMIZED)
			break;
		g_pImmediateContext->OMSetRenderTargets(0, NULL, NULL); //�`��^�[�Q�b�g�̉���
		SAFE_RELEASE(g_pRenderTargetView); //�`��^�[�Q�b�g�r���[�̊J��

		g_pSwapChain->ResizeBuffers(1, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0);//�o�b�t�@�̕ύX

		//�o�b�N�o�b�t�@�̏�����
		InitBackBuffer();
		break;

	case WM_KEYDOWN: //�L�[�̓��͏���
		switch (wParam) {
		case VK_ESCAPE:
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		case VK_F5: //��ʃ��[�h�̐؂�ւ�
			if (g_pSwapChain != NULL) {
				g_pSwapChain->GetFullscreenState(&fullscreen, NULL);
				g_pSwapChain->SetFullscreenState(!fullscreen, NULL);
			}
			break;
		}
	}
	//�f�t�H���g����
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//�E�B���h�E����
void InitWindow() {
	WNDCLASSEX wc = {
		sizeof(WNDCLASSEX),
		CS_CLASSDC,
		WindowProc,
		0L,
		0L,
		GetModuleHandle(NULL),
		NULL,
		LoadCursor(NULL, IDC_ARROW),
		(HBRUSH)GetStockObject(WHITE_BRUSH),
		NULL,
		L"Sample",
		NULL
	};
	RegisterClassEx(&wc);

	g_Hwnd = CreateWindow(
		L"Sample",
		L"DirectX11 sample",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		WINDOW_W,
		WINDOW_H,
		NULL,
		NULL,
		wc.hInstance,
		NULL
	);
}

//D3D������
HRESULT InitD3D() {
	HRESULT hr = S_OK;

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1; //�o�b�N�o�b�t�@�̐�
	sd.BufferDesc.Width = WINDOW_W; //�o�b�N�o�b�t�@�̕�
	sd.BufferDesc.Height = WINDOW_H; //�o�b�N�o�b�t�@�̍���
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //�t�H�[�}�b�g
	sd.BufferDesc.RefreshRate.Numerator = 60; //���t���b�V�����[�g(���q)
	sd.BufferDesc.RefreshRate.Denominator = 1; //���t���b�V�����[�g(����)
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //�o�b�N�o�b�t�@�̎g�p���@
	sd.OutputWindow = g_Hwnd; //�֘A�t����E�B���h�E
	sd.SampleDesc.Count = 1; //�}���`�T���v���̐�
	sd.SampleDesc.Quality = 0; //�}���`�T���v���̃N�I���e�B
	sd.Windowed = TRUE; //�E�B���h�E���[�h
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; //���[�h�����؂�ւ�

	//�f�o�C�X�ƃX���b�v�`�F�C���̍쐬
	hr = D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		g_pFeatureLevels,
		1,
		D3D11_SDK_VERSION,
		&sd,
		&g_pSwapChain,
		&g_pD3DDevice,
		NULL,
		&g_pImmediateContext
	);

	if (FAILED(hr)) {
		hr = D3D11CreateDeviceAndSwapChain(
			NULL,
			D3D_DRIVER_TYPE_WARP,
			NULL,
			0,
			g_pFeatureLevels,
			1,
			D3D11_SDK_VERSION,
			&sd,
			&g_pSwapChain,
			&g_pD3DDevice,
			NULL,
			&g_pImmediateContext
		);

		if (FAILED(hr)) {
			hr = D3D11CreateDeviceAndSwapChain(
				NULL,
				D3D_DRIVER_TYPE_REFERENCE,
				NULL,
				0,
				g_pFeatureLevels,
				1,
				D3D11_SDK_VERSION,
				&sd,
				&g_pSwapChain,
				&g_pD3DDevice,
				NULL,
				&g_pImmediateContext
			);

			if (FAILED(hr))
				return E_FAIL;
		}
	}

	//�o�b�N�o�b�t�@��`��^�[�Q�b�g�ɐݒ�
	//�X���b�v�`�F�C������ŏ��̃o�b�N�o�b�t�@���擾����
	ID3D11Texture2D* pBackBuffer; //�o�b�t�@�̃A�N�Z�X�Ɏg���C���^�[�t�F�C�X(�o�b�t�@��2D�e�N�X�`���Ƃ��Ĉ���)
	hr = g_pSwapChain->GetBuffer(
		0,
		__uuidof(ID3D11Texture2D), //�o�b�t�@�ɃA�N�Z�X����C���^�[�t�F�C�X
		(LPVOID*)&pBackBuffer //�o�b�t�@���󂯎��ϐ�
	);
	if (FAILED(hr))
		return E_FAIL;

	//�`��^�[�Q�b�g�r���[���o�̓}�[�W���[�̕`��^�[�Q�b�g�r���[�Ƃ��Đݒ�
	g_pImmediateContext->OMSetRenderTargets(
		1, //�`��^�[�Q�b�g�̐�
		&g_pRenderTargetView, //�`��^�[�Q�b�g�r���[�̔z��
		NULL //�[�x/�X�e���V���r���[��ݒ肵�Ȃ�
	);

	//�r���[�|�[�g�̐ݒ�(�`��̈�)
	g_ViewPort[0].TopLeftX = 0.0f; //�r���[�|�[�g�̈�̍��゘���W
	g_ViewPort[0].TopLeftY = 0.0f; //�r���[�|�[�g�̈�̍��゙���W
	g_ViewPort[0].Width = WINDOW_W; //�r���[�|�[�g�̈�̕�
	g_ViewPort[0].Height = WINDOW_H; //�r���[�|�[�g�̈�̍���
	g_ViewPort[0].MinDepth = 0.0f; //�r���[�|�[�g�̈�̐[�x�l�̍ŏ��l
	g_ViewPort[0].MaxDepth = 1.0f; //�r���[�|�[�g�̈�̐[�x�l�̍ő�l
	//���X�^���C�U�Ƀr���[�|�[�g��ݒ�
	g_pImmediateContext->RSSetViewports(1, g_ViewPort);

	//�[�x/�X�e���V���E�e�N�X�`���̍쐬
	D3D11_TEXTURE2D_DESC descDepth;
	descDepth.Width = WINDOW_W; //��
	descDepth.Height = WINDOW_H; //����
	descDepth.MipLevels = 1; //�~�b�v�}�b�v�@���x����
	descDepth.ArraySize = 1; //�z��T�C�Y
	descDepth.Format = DXGI_FORMAT_D32_FLOAT; //�t�H�[�}�b�g(�[�x�̂�)
	descDepth.SampleDesc.Count = 1; //�}���`�T���v�����O�̐ݒ�
	descDepth.SampleDesc.Quality = 0; //�}���`�T���v�����O�̕i��
	descDepth.Usage = D3D11_USAGE_DEFAULT; //�g�p���@�@�f�t�H���g
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL; //�[�x/�X�e���V���Ƃ��Ďg�p
	descDepth.CPUAccessFlags = 0; //CPU����A�N�Z�X���Ȃ�
	descDepth.MiscFlags = 0; //���̑��̐ݒ�Ȃ�
	hr = g_pD3DDevice->CreateTexture2D(
		&descDepth, //�쐬����2D�e�N�X�`��
		NULL,
		&g_pDepthStencil //�쐬�����e�N�X�`�����󂯎��
	);
	if (FAILED(hr))
		return E_FAIL;

	//�[�x/�X�e���V���r���[�̍쐬
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Flags = 0;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pD3DDevice->CreateDepthStencilView(
		g_pDepthStencil, //�[�x/�X�e���V���r���[�����e�N�X�`��
		&descDSV, //�[�x/�X�e���V���r���[�̐ݒ�
		&g_pDepthStencilView //�쐬�����r���[���󂯎��
	);
	if (FAILED(hr))
		return E_FAIL;

	return S_OK;
}

HRESULT InitBackBuffer() {
	HRESULT hr;

	//�o�b�N�o�b�t�@��`��^�[�Q�b�g�ɐݒ�
	//�X���b�v�`�F�C������ŏ��̃o�b�N�o�b�t�@���擾����
	ID3D11Texture2D* pBackBuffer; //�o�b�t�@�̃A�N�Z�X�Ɏg���C���^�[�t�F�C�X(�o�b�t�@��2D�e�N�X�`���Ƃ��Ĉ���)
	hr = g_pSwapChain->GetBuffer(
		0,
		__uuidof(ID3D11Texture2D), //�o�b�t�@�ɃA�N�Z�X����C���^�[�t�F�C�X
		(LPVOID*)&pBackBuffer //�o�b�t�@���󂯎��ϐ�
	);
	if (FAILED(hr))
		return E_FAIL;

	//�o�b�N�o�b�t�@�̕`��^�[�Q�b�g�r���[�����
	hr = g_pD3DDevice->CreateRenderTargetView(
		pBackBuffer, //�r���[�ŃA�N�Z�X���郊�\�[�X
		NULL, //�`��^�[�Q�b�g�r���[�̒�`
		&g_pRenderTargetView //�`��^�[�Q�b�g�r���[���󂯎��֐�
	);
	SAFE_RELEASE(pBackBuffer); //�ȍ~�o�b�N�o�b�t�@�͒��ڎg��Ȃ��̂ŉ��U
	if (FAILED(hr))
		return E_FAIL;

	//�r���[�|�[�g�̐ݒ�
	g_ViewPort[0].TopLeftX = 0.0f;
	g_ViewPort[0].TopLeftY = 0.0f;
	g_ViewPort[0].Width = 640.0f;
	g_ViewPort[0].Height = 480.0f;
	g_ViewPort[0].MinDepth = 0.0f;
	g_ViewPort[0].MaxDepth = 1.0f;

	return S_OK;
}

//�N���A
void Clear() {
	//�`��^�[�Q�b�g�̃N���A
	float clearcolor[4] = { 0.0f,0.125f,0.3f,1.0f }; //�N���A����l
	g_pImmediateContext->ClearRenderTargetView(
		g_pRenderTargetView, //�N���A����`��^�[�Q�b�g
		clearcolor //�N���A����l
	);

	//�[�x/�X�e���V���l�̃N���A
	g_pImmediateContext->ClearDepthStencilView(
		g_pDepthStencilView, //�N���A����[�x/�X�e���V���r���[
		D3D11_CLEAR_DEPTH, //�[�x�l�����N���A����
		1.0f, //�[�x�o�b�t�@���N���A����l
		0 //�X�e���V���o�b�t�@���N���A����l
	);
}

//�`��
HRESULT Flip() {
	//�o�b�N�o�b�t�@�̕\��
	HRESULT hr = g_pSwapChain->Present(
		0, //��ʂ������ɍX�V����
		0 //��ʂ����ۂɍX�V����
	);
	return hr;
}

//�f�o�C�X��������
bool DeviceRemoveProc() {
	HRESULT hr = g_pD3DDevice->GetDeviceRemovedReason();
	switch (hr) {
	case S_OK:
		break;
	case DXGI_ERROR_DEVICE_HUNG:
	case DXGI_ERROR_DEVICE_RESET:
		CleanupD3D();
		hr = InitD3D();
		if (FAILED(hr))
			return true;
	case DXGI_ERROR_DEVICE_REMOVED:
	case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
	case DXGI_ERROR_INVALID_CALL:
	default:
		return true;
	}
	return false;
}

//�㏈��
void CleanupD3D() {
	//�X�e�[�g�̃N���A
	if (g_pImmediateContext)
		g_pImmediateContext->ClearState();

	//�X���b�v�`�F�C�����E�B���h�E���[�h�ɂ���
	if (g_pSwapChain)
		g_pSwapChain->SetFullscreenState(FALSE, NULL);

	//�C���^�[�t�F�C�X�̉��
	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencil);
	SAFE_RELEASE(g_pRenderTargetView);
	SAFE_RELEASE(g_pSwapChain);
	SAFE_RELEASE(g_pImmediateContext);
	SAFE_RELEASE(g_pD3DDevice);
}
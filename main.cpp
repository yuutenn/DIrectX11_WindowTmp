#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "dxgi.lib")


#include <tchar.h>
#include <Windows.h>
#include <D3D11.h>
#include <D3DX11.h>
#include <DxErr.h>

//COMポインタの開放処理マクロ
#define SAFE_RELEASE(x) {if(x){(x)->Release(); (x)=nullptr;}}

#define WINDOW_W 640 //ウィンドウの幅
#define WINDOW_H 480 //ウィンドウの高さ

HWND g_Hwnd; //ウィンドウ

//機能レベル
D3D_FEATURE_LEVEL g_pFeatureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
UINT g_FeatureLevels = 3; //配列の要素数
D3D_FEATURE_LEVEL g_FeatureLevelsSupported; //デバイス作成時に返される機能レベル

//インターフェイス
ID3D11Device* g_pD3DDevice = nullptr; //デバイス
ID3D11DeviceContext* g_pImmediateContext = nullptr; //デバイス・コンテキスト
IDXGISwapChain* g_pSwapChain = nullptr; //スワップチェイン

ID3D11RenderTargetView* g_pRenderTargetView = nullptr; //描画ターゲット・ビュー
D3D11_VIEWPORT g_ViewPort[1]; //ビューポート
ID3D11Texture2D* g_pDepthStencil; //深度/ステンシル・テクスチャ
ID3D11DepthStencilView* g_pDepthStencilView = nullptr; //深度/ステンシルビュー

//プロトタイプ宣言
void InitWindow(); //ウィンドウ生成
HRESULT InitD3D(); //D3D初期化
HRESULT InitBackBuffer(); //バックバッファの初期化
void Clear(); //画面クリア
HRESULT Flip(); //画面表示
void CleanupD3D(); //D3D後処理
bool DeviceRemoveProc(); //デバイス消失処理


//エントリポイント
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
			//デバイス消失処理
			if (DeviceRemoveProc()) break;

			//ゲームループ
			Clear();
			Flip();
		}
	}

	//解放
	CleanupD3D();
	return 0;
}

//ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	
	HRESULT hr;
	BOOL fullscreen;
	
	//メッセージ処理
	switch (uMsg) {
	case WM_DESTROY:
		//Direct3Dの終了処理
		CleanupD3D();
		//ウィンドウを閉じる
		PostQuitMessage(0);
		g_Hwnd = NULL;
		break;

	case WM_SIZE: //サイズ変更処理
		if (!g_pD3DDevice || wParam == SIZE_MINIMIZED)
			break;
		g_pImmediateContext->OMSetRenderTargets(0, NULL, NULL); //描画ターゲットの解除
		SAFE_RELEASE(g_pRenderTargetView); //描画ターゲットビューの開放

		g_pSwapChain->ResizeBuffers(1, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0);//バッファの変更

		//バックバッファの初期化
		InitBackBuffer();
		break;

	case WM_KEYDOWN: //キーの入力処理
		switch (wParam) {
		case VK_ESCAPE:
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		case VK_F5: //画面モードの切り替え
			if (g_pSwapChain != NULL) {
				g_pSwapChain->GetFullscreenState(&fullscreen, NULL);
				g_pSwapChain->SetFullscreenState(!fullscreen, NULL);
			}
			break;
		}
	}
	//デフォルト処理
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//ウィンドウ生成
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

//D3D初期化
HRESULT InitD3D() {
	HRESULT hr = S_OK;

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1; //バックバッファの数
	sd.BufferDesc.Width = WINDOW_W; //バックバッファの幅
	sd.BufferDesc.Height = WINDOW_H; //バックバッファの高さ
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //フォーマット
	sd.BufferDesc.RefreshRate.Numerator = 60; //リフレッシュレート(分子)
	sd.BufferDesc.RefreshRate.Denominator = 1; //リフレッシュレート(分母)
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //バックバッファの使用方法
	sd.OutputWindow = g_Hwnd; //関連付けるウィンドウ
	sd.SampleDesc.Count = 1; //マルチサンプルの数
	sd.SampleDesc.Quality = 0; //マルチサンプルのクオリティ
	sd.Windowed = TRUE; //ウィンドウモード
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; //モード自動切り替え

	//デバイスとスワップチェインの作成
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

	//バックバッファを描画ターゲットに設定
	//スワップチェインから最初のバックバッファを取得する
	ID3D11Texture2D* pBackBuffer; //バッファのアクセスに使うインターフェイス(バッファを2Dテクスチャとして扱う)
	hr = g_pSwapChain->GetBuffer(
		0,
		__uuidof(ID3D11Texture2D), //バッファにアクセスするインターフェイス
		(LPVOID*)&pBackBuffer //バッファを受け取る変数
	);
	if (FAILED(hr))
		return E_FAIL;

	//描画ターゲットビューを出力マージャーの描画ターゲットビューとして設定
	g_pImmediateContext->OMSetRenderTargets(
		1, //描画ターゲットの数
		&g_pRenderTargetView, //描画ターゲットビューの配列
		NULL //深度/ステンシルビューを設定しない
	);

	//ビューポートの設定(描画領域)
	g_ViewPort[0].TopLeftX = 0.0f; //ビューポート領域の左上ｘ座標
	g_ViewPort[0].TopLeftY = 0.0f; //ビューポート領域の左上ｙ座標
	g_ViewPort[0].Width = WINDOW_W; //ビューポート領域の幅
	g_ViewPort[0].Height = WINDOW_H; //ビューポート領域の高さ
	g_ViewPort[0].MinDepth = 0.0f; //ビューポート領域の深度値の最小値
	g_ViewPort[0].MaxDepth = 1.0f; //ビューポート領域の深度値の最大値
	//ラスタライザにビューポートを設定
	g_pImmediateContext->RSSetViewports(1, g_ViewPort);

	//深度/ステンシル・テクスチャの作成
	D3D11_TEXTURE2D_DESC descDepth;
	descDepth.Width = WINDOW_W; //幅
	descDepth.Height = WINDOW_H; //高さ
	descDepth.MipLevels = 1; //ミップマップ　レベル数
	descDepth.ArraySize = 1; //配列サイズ
	descDepth.Format = DXGI_FORMAT_D32_FLOAT; //フォーマット(深度のみ)
	descDepth.SampleDesc.Count = 1; //マルチサンプリングの設定
	descDepth.SampleDesc.Quality = 0; //マルチサンプリングの品質
	descDepth.Usage = D3D11_USAGE_DEFAULT; //使用方法　デフォルト
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL; //深度/ステンシルとして使用
	descDepth.CPUAccessFlags = 0; //CPUからアクセスしない
	descDepth.MiscFlags = 0; //その他の設定なし
	hr = g_pD3DDevice->CreateTexture2D(
		&descDepth, //作成する2Dテクスチャ
		NULL,
		&g_pDepthStencil //作成したテクスチャを受け取る
	);
	if (FAILED(hr))
		return E_FAIL;

	//深度/ステンシルビューの作成
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Flags = 0;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pD3DDevice->CreateDepthStencilView(
		g_pDepthStencil, //深度/ステンシルビューを作るテクスチャ
		&descDSV, //深度/ステンシルビューの設定
		&g_pDepthStencilView //作成したビューを受け取る
	);
	if (FAILED(hr))
		return E_FAIL;

	return S_OK;
}

HRESULT InitBackBuffer() {
	HRESULT hr;

	//バックバッファを描画ターゲットに設定
	//スワップチェインから最初のバックバッファを取得する
	ID3D11Texture2D* pBackBuffer; //バッファのアクセスに使うインターフェイス(バッファを2Dテクスチャとして扱う)
	hr = g_pSwapChain->GetBuffer(
		0,
		__uuidof(ID3D11Texture2D), //バッファにアクセスするインターフェイス
		(LPVOID*)&pBackBuffer //バッファを受け取る変数
	);
	if (FAILED(hr))
		return E_FAIL;

	//バックバッファの描画ターゲットビューを作る
	hr = g_pD3DDevice->CreateRenderTargetView(
		pBackBuffer, //ビューでアクセスするリソース
		NULL, //描画ターゲットビューの定義
		&g_pRenderTargetView //描画ターゲットビューを受け取る関数
	);
	SAFE_RELEASE(pBackBuffer); //以降バックバッファは直接使わないので解散
	if (FAILED(hr))
		return E_FAIL;

	//ビューポートの設定
	g_ViewPort[0].TopLeftX = 0.0f;
	g_ViewPort[0].TopLeftY = 0.0f;
	g_ViewPort[0].Width = 640.0f;
	g_ViewPort[0].Height = 480.0f;
	g_ViewPort[0].MinDepth = 0.0f;
	g_ViewPort[0].MaxDepth = 1.0f;

	return S_OK;
}

//クリア
void Clear() {
	//描画ターゲットのクリア
	float clearcolor[4] = { 0.0f,0.125f,0.3f,1.0f }; //クリアする値
	g_pImmediateContext->ClearRenderTargetView(
		g_pRenderTargetView, //クリアする描画ターゲット
		clearcolor //クリアする値
	);

	//深度/ステンシル値のクリア
	g_pImmediateContext->ClearDepthStencilView(
		g_pDepthStencilView, //クリアする深度/ステンシルビュー
		D3D11_CLEAR_DEPTH, //深度値だけクリアする
		1.0f, //深度バッファをクリアする値
		0 //ステンシルバッファをクリアする値
	);
}

//描画
HRESULT Flip() {
	//バックバッファの表示
	HRESULT hr = g_pSwapChain->Present(
		0, //画面をすぐに更新する
		0 //画面を実際に更新する
	);
	return hr;
}

//デバイス消失処理
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

//後処理
void CleanupD3D() {
	//ステートのクリア
	if (g_pImmediateContext)
		g_pImmediateContext->ClearState();

	//スワップチェインをウィンドウモードにする
	if (g_pSwapChain)
		g_pSwapChain->SetFullscreenState(FALSE, NULL);

	//インターフェイスの解放
	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencil);
	SAFE_RELEASE(g_pRenderTargetView);
	SAFE_RELEASE(g_pSwapChain);
	SAFE_RELEASE(g_pImmediateContext);
	SAFE_RELEASE(g_pD3DDevice);
}
#pragma once

auto LoadPNGFromResource()
{
	auto hRes = FindResourceW(g_hInst, MAKEINTRESOURCEW(1), L"PNG");
	THROW_LAST_ERROR_IF_NULL(hRes);

	auto hResData = LoadResource(g_hInst, hRes);
	THROW_LAST_ERROR_IF_NULL(hResData);

	auto data = LockResource(hResData);
	THROW_IF_NULL_ALLOC(data);

	auto size = SizeofResource(g_hInst, hRes);
	THROW_LAST_ERROR_IF(size == 0);

	auto factory = wil::CoCreateInstance<IWICImagingFactory>(CLSID_WICImagingFactory);

	wil::com_ptr<IWICStream> stream;
	THROW_IF_FAILED(factory->CreateStream(stream.put()));
	THROW_IF_FAILED(stream->InitializeFromMemory(reinterpret_cast<WICInProcPointer>(data), size));

	wil::com_ptr<IWICBitmapDecoder> decoder;
	THROW_IF_FAILED(factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.put()));

	wil::com_ptr<IWICBitmapFrameDecode> frame;
	THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));

	return frame.query<IWICBitmapSource>();
}

auto CreateDIB(HDC hdc, LONG width, LONG height, WORD bitCount, void** bitmapBits)
{
	BITMAPINFO bmi = { .bmiHeader = {
		.biSize = sizeof(BITMAPINFOHEADER),
		.biWidth = width,
		.biHeight = -height, // negative is top-down bitmap
		.biPlanes = 1,
		.biBitCount = bitCount,
		.biCompression = BI_RGB,
	} };
	return wil::unique_hbitmap(CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, bitmapBits, nullptr, 0));
}

HICON CreateIconFromWICBitmap(HDC hdc, IWICBitmapSource* bitmap, uint8_t alpha, bool inverse)
{
	WICPixelFormatGUID pixelFormat;
	THROW_IF_FAILED(bitmap->GetPixelFormat(&pixelFormat));
	THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_PIXEL_FORMAT), pixelFormat != GUID_WICPixelFormat32bppBGRA);

	UINT width, height;
	THROW_IF_FAILED(bitmap->GetSize(&width, &height));

	RGBQUAD* bmBits;
	auto hBitmap = CreateDIB(hdc, width, height, 32, reinterpret_cast<void**>(&bmBits));
	THROW_IF_NULL_ALLOC(hBitmap);

	UINT stride = 0;
	THROW_IF_FAILED(UIntMult(width, sizeof(RGBQUAD), &stride)); // check overflow
	UINT bufSize = 0;
	THROW_IF_FAILED(UIntMult(stride, height, &bufSize));

	THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, bufSize, reinterpret_cast<BYTE*>(bmBits)));

	// PNG uses non-premultiplied alpha but GDI uses premultiplied,
	// here we modify alpha value and premultiply.
	if (inverse)
	{
		for (UINT y = 0; y < height; ++y)
		{
			for (UINT x = 0; x < width; ++x)
			{
				auto pixel = &bmBits[x + y * width];
				auto pixelAlpha = static_cast<BYTE>(pixel->rgbReserved * alpha / 255);
				pixel->rgbBlue = static_cast<BYTE>((255 - pixel->rgbBlue) * pixelAlpha / 255);
				pixel->rgbGreen = static_cast<BYTE>((255 - pixel->rgbGreen) * pixelAlpha / 255);
				pixel->rgbRed = static_cast<BYTE>((255 - pixel->rgbRed) * pixelAlpha / 255);
				pixel->rgbReserved = pixelAlpha;
			}
		}
	}
	else
	{
		for (UINT y = 0; y < height; ++y)
		{
			for (UINT x = 0; x < width; ++x)
			{
				auto pixel = &bmBits[x + y * width];
				auto pixelAlpha = static_cast<BYTE>(pixel->rgbReserved * alpha / 255);
				pixel->rgbBlue = static_cast<BYTE>(pixel->rgbBlue * pixelAlpha / 255);
				pixel->rgbGreen = static_cast<BYTE>(pixel->rgbGreen * pixelAlpha / 255);
				pixel->rgbRed = static_cast<BYTE>(pixel->rgbRed * pixelAlpha / 255);
				pixel->rgbReserved = pixelAlpha;
			}
		}
	}

	auto hBitmapMask = CreateDIB(hdc, width, height, 1, nullptr);
	THROW_IF_NULL_ALLOC(hBitmapMask);

	ICONINFO iconInfo = {
		.fIcon = TRUE,
		.hbmMask = hBitmapMask.get(),
		.hbmColor = hBitmap.get() // system doesn't take ownership
	};
	HICON hIcon = CreateIconIndirect(&iconInfo);
	THROW_LAST_ERROR_IF_NULL(hIcon);

	return hIcon;
}

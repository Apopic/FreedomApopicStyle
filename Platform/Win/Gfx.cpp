// 描画 (Direct3D 12)。
// 頂点はフレームごとのアップロード用バッファにまとめて書き、合成モード (と、バインドレスが使えない GPU ではテクスチャ) が
// 変わる時だけ描画命令を出す。表示は待機可能なスワップチェーン (遅延 1 フレーム) で、VSync を切った時はティアリングを許す。
#include "WinInternal.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace gfx {
    struct TextureObject {
        ComPtr<ID3D12Resource> Res;
        int W = 0, H = 0;
        uint32_t Srv = UINT32_MAX;          // シェーダーから見える記述子ヒープの番号
        uint32_t Rtv = UINT32_MAX;          // 描画先にできる時だけ
        D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COPY_DEST;
        ~TextureObject();
    };
}

namespace gfx {
    namespace {
        constexpr UINT FrameCount = 2;
        constexpr UINT BackBufferCount = 2;
        constexpr UINT SrvCapacity = 32768;
        constexpr UINT RtvCapacity = 256;
        constexpr DXGI_FORMAT Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        constexpr size_t PageSize = 8u << 20;
        constexpr size_t RestartUploadBytes = 192u << 20;   // 1 フレームでこれ以上送ったら一度 GPU に渡す (読み込み中など)
        constexpr size_t MaxBatchVertices = 6 * 16384;
        enum Pso { PsoOpaque, PsoAlpha, PsoAdd, PsoSub, PsoMul, PsoCount };

        struct Vertex {
            float X, Y, U, V;
            uint32_t Color;     // R8G8B8A8 (明るさ・アルファ)
            uint32_t Add;       // R8G8B8A8 (画像に足す色)
            uint32_t Tex;       // バインドレスの時のテクスチャ番号
        };
        static_assert(sizeof(Vertex) == 28);

        struct UploadPage { ComPtr<ID3D12Resource> Res; uint8_t* Cpu = nullptr; D3D12_GPU_VIRTUAL_ADDRESS Gpu = 0; size_t Size = 0, Used = 0; };
        struct UploadSlice { ID3D12Resource* Res = nullptr; UINT64 Offset = 0; uint8_t* Cpu = nullptr; D3D12_GPU_VIRTUAL_ADDRESS Gpu = 0; };
        struct Frame {
            ComPtr<ID3D12CommandAllocator> Alloc;
            uint64_t Fence = 0;
            std::vector<UploadPage> Pages;
            std::vector<ComPtr<ID3D12Resource>> Big;    // 1 ページに入らない大きなアップロード
        };
        struct Garbage { uint64_t Fence; ComPtr<ID3D12Resource> Res; uint32_t Srv, Rtv; };
        struct DescriptorPool {
            std::vector<uint32_t> Free;
            uint32_t Next = 0, Capacity = 0;
            uint32_t Alloc() {
                if (!Free.empty()) { const uint32_t i = Free.back(); Free.pop_back(); return i; }
                return Next < Capacity ? Next++ : UINT32_MAX;
            }
            void Release(uint32_t i) { if (i != UINT32_MAX) { Free.push_back(i); } }
        };

        struct State {
            bool Alive = false;
            HWND Hwnd = nullptr;
            ComPtr<IDXGIFactory4> Factory;
            ComPtr<ID3D12Device> Device;
            ComPtr<ID3D12CommandQueue> Queue;
            ComPtr<IDXGISwapChain3> Swap;
            HANDLE Waitable = nullptr;
            ComPtr<ID3D12Resource> Back[BackBufferCount];
            uint32_t BackRtv[BackBufferCount]{};
            int BackW = 0, BackH = 0;
            bool Tearing = false, VSync = true, ResizePending = false;

            ComPtr<ID3D12DescriptorHeap> SrvHeap, RtvHeap;
            UINT SrvInc = 0, RtvInc = 0;
            DescriptorPool SrvPool, RtvPool;
            ComPtr<ID3D12RootSignature> Root;
            ComPtr<ID3D12PipelineState> Psos[PsoCount];
            bool Bindless = false;

            ComPtr<ID3D12GraphicsCommandList> List;
            ComPtr<ID3D12Fence> Fence;
            HANDLE FenceEvent = nullptr;
            uint64_t FenceValue = 0;            // 最後に Signal した値
            Frame Frames[FrameCount];
            UINT FrameIndex = 0;
            bool Recording = false;
            size_t FrameUploadBytes = 0;
            std::deque<Garbage> Trash;
            ComPtr<ID3D12Resource> Readback;
            size_t ReadbackSize = 0;
            // 待たない読み出し (録画) の読み出し用バッファ。GPU が終わったものから古い順に受け取る
            struct ReadSlot { ComPtr<ID3D12Resource> Res; size_t Size = 0, Pitch = 0; uint64_t Fence = 0, Order = 0; int W = 0, H = 0; bool Pending = false; };
            std::vector<ReadSlot> ReadSlots;
            uint64_t ReadOrder = 0;

            int LogicalW = 1280, LogicalH = 720;
            Blend Mode = Blend::None;
            int Param = 255;
            int BrightR = 255, BrightG = 255, BrightB = 255;
            int AddR = 0, AddG = 0, AddB = 0;
            Texture Canvas, White;
            const Texture* Target = nullptr;
            bool ClipOn = false;
            D3D12_RECT Clip{};

            std::vector<Vertex> Batch;
            int BatchPso = -1;
            uint32_t BatchSrv = UINT32_MAX;
            int BoundPso = -1;
            uint32_t BoundSrv = UINT32_MAX;
            std::string Error;
            ComPtr<ID3D12InfoQueue> Debug;      // FAS_D3D12_DEBUG=1 の時だけ (検証レイヤーの報告を d3d12_debug.log に書く)
        } S;

        // 図形の組み立て用 (使い回して確保を減らす)
        struct PrimVertex { float X, Y; uint32_t C; };
        std::vector<PrimVertex> PrimVerts;
        std::vector<int> PrimIndices;

        void DumpDebugMessages() {
            if (!S.Debug) { return; }
            const UINT64 n = S.Debug->GetNumStoredMessages();
            if (n == 0) { return; }
            FILE* f = nullptr;
            if (fopen_s(&f, "d3d12_debug.log", "a") == 0 && f) {
                for (UINT64 i = 0; i < n; ++i) {
                    SIZE_T len = 0;
                    S.Debug->GetMessage(i, nullptr, &len);
                    std::vector<char> buf(len);
                    auto* m = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
                    if (SUCCEEDED(S.Debug->GetMessage(i, m, &len))) { std::fprintf(f, "[%d] %s\n", (int)m->Severity, m->pDescription); }
                }
                std::fclose(f);
            }
            S.Debug->ClearStoredMessages();
        }
        bool Fail(const char* what, HRESULT hr) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s (HRESULT 0x%08X)", what, (unsigned)hr);
            S.Error = buf;
            return false;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE SrvCpu(uint32_t i) { auto h = S.SrvHeap->GetCPUDescriptorHandleForHeapStart(); h.ptr += (SIZE_T)i * S.SrvInc; return h; }
        D3D12_GPU_DESCRIPTOR_HANDLE SrvGpu(uint32_t i) { auto h = S.SrvHeap->GetGPUDescriptorHandleForHeapStart(); h.ptr += (UINT64)i * S.SrvInc; return h; }
        D3D12_CPU_DESCRIPTOR_HANDLE RtvCpu(uint32_t i) { auto h = S.RtvHeap->GetCPUDescriptorHandleForHeapStart(); h.ptr += (SIZE_T)i * S.RtvInc; return h; }
        size_t AlignUp(size_t v, size_t a) { return (v + a - 1) / a * a; }
        uint32_t Pack(float r, float g, float b, float a) {
            auto q = [](float v) { return (uint32_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f); };
            return q(r) | (q(g) << 8) | (q(b) << 16) | (q(a) << 24);
        }
        float Alpha() { return (S.Mode == Blend::None ? 255 : std::clamp(S.Param, 0, 255)) / 255.0f; }
        int PsoFor(Blend b) {
            switch (b) {
            case Blend::Add: return PsoAdd;
            case Blend::Sub: return PsoSub;
            case Blend::Mul: return PsoMul;
            default: return PsoAlpha;
            }
        }
        TextureObject* TargetObject() { return S.Target && S.Target->Tex ? S.Target->Tex.get() : S.Canvas.Tex.get(); }

        void WaitFence(uint64_t value) {
            if (value == 0 || S.Fence->GetCompletedValue() >= value) { return; }
            S.Fence->SetEventOnCompletion(value, S.FenceEvent);
            WaitForSingleObject(S.FenceEvent, INFINITE);
        }
        void WaitIdle() {
            if (!S.Queue || !S.Fence) { return; }
            S.Queue->Signal(S.Fence.Get(), ++S.FenceValue);
            WaitFence(S.FenceValue);
        }
        void CollectTrash() {
            const uint64_t done = S.Fence->GetCompletedValue();
            while (!S.Trash.empty() && S.Trash.front().Fence <= done) {
                S.SrvPool.Release(S.Trash.front().Srv);
                S.RtvPool.Release(S.Trash.front().Rtv);
                S.Trash.pop_front();
            }
        }
        void Transition(TextureObject* t, D3D12_RESOURCE_STATES to) {
            if (!t || t->State == to) { return; }
            D3D12_RESOURCE_BARRIER b{};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.pResource = t->Res.Get();
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore = t->State;
            b.Transition.StateAfter = to;
            S.List->ResourceBarrier(1, &b);
            t->State = to;
        }
        void BackBarrier(ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
            D3D12_RESOURCE_BARRIER b{};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.pResource = r;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore = from;
            b.Transition.StateAfter = to;
            S.List->ResourceBarrier(1, &b);
        }

        UploadSlice Upload(size_t size, size_t align) {
            Frame& f = S.Frames[S.FrameIndex];
            S.FrameUploadBytes += size;
            auto make = [](size_t bytes, UploadPage& page) {
                D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_UPLOAD };
                D3D12_RESOURCE_DESC rd{};
                rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                rd.Width = bytes; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
                rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                if (FAILED(S.Device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&page.Res)))) { return false; }
                D3D12_RANGE none{ 0, 0 };
                void* p = nullptr;
                page.Res->Map(0, &none, &p);
                page.Cpu = (uint8_t*)p;
                page.Gpu = page.Res->GetGPUVirtualAddress();
                page.Size = bytes;
                page.Used = 0;
                return page.Cpu != nullptr;
            };
            if (size > PageSize / 2) {
                UploadPage big;
                if (!make(AlignUp(size, 65536), big)) { return {}; }
                f.Big.push_back(big.Res);
                return { big.Res.Get(), 0, big.Cpu, big.Gpu };
            }
            for (auto& p : f.Pages) {
                const size_t off = AlignUp(p.Used, align);
                if (off + size <= p.Size) { p.Used = off + size; return { p.Res.Get(), off, p.Cpu + off, p.Gpu + off }; }
            }
            UploadPage page;
            if (!make(PageSize, page)) { return {}; }
            page.Used = size;
            f.Pages.push_back(page);
            auto& p = f.Pages.back();
            return { p.Res.Get(), 0, p.Cpu, p.Gpu };
        }

        void ApplyScissor() {
            TextureObject* t = TargetObject();
            if (!t) { return; }
            D3D12_RECT r{ 0, 0, t->W, t->H };
            if (S.ClipOn) {
                r.left = std::clamp(S.Clip.left, 0L, (LONG)t->W);
                r.top = std::clamp(S.Clip.top, 0L, (LONG)t->H);
                r.right = std::clamp(S.Clip.right, r.left, (LONG)t->W);
                r.bottom = std::clamp(S.Clip.bottom, r.top, (LONG)t->H);
            }
            S.List->RSSetScissorRects(1, &r);
        }
        void SetViewport(int w, int h) {
            const D3D12_VIEWPORT vp{ 0, 0, (float)w, (float)h, 0, 1 };
            S.List->RSSetViewports(1, &vp);
            const float c[4] = { 2.0f / w, -2.0f / h, -1.0f, 1.0f };
            S.List->SetGraphicsRoot32BitConstants(0, 4, c, 0);
        }
        void BindTarget() {
            TextureObject* t = TargetObject();
            if (!t) { return; }
            Transition(t, D3D12_RESOURCE_STATE_RENDER_TARGET);
            const auto rtv = RtvCpu(t->Rtv);
            S.List->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            SetViewport(t->W, t->H);
            ApplyScissor();
        }
        void Begin() {
            if (S.Recording || !S.Alive) { return; }
            Frame& f = S.Frames[S.FrameIndex];
            WaitFence(f.Fence);
            CollectTrash();
            f.Alloc->Reset();
            S.List->Reset(f.Alloc.Get(), nullptr);
            if (f.Pages.size() > 2) { f.Pages.resize(2); }     // 読み込みなどで増えた分は返す (GPU はこのフレームを使い終えている)
            for (auto& p : f.Pages) { p.Used = 0; }
            f.Big.clear();
            S.FrameUploadBytes = 0;
            S.Recording = true;
            ID3D12DescriptorHeap* heaps[] = { S.SrvHeap.Get() };
            S.List->SetDescriptorHeaps(1, heaps);
            S.List->SetGraphicsRootSignature(S.Root.Get());
            S.List->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            if (S.Bindless) { S.List->SetGraphicsRootDescriptorTable(1, S.SrvHeap->GetGPUDescriptorHandleForHeapStart()); }
            S.BoundPso = -1;
            S.BoundSrv = UINT32_MAX;
            BindTarget();
        }
        void Flush() {
            if (S.Batch.empty()) { return; }
            const size_t bytes = S.Batch.size() * sizeof(Vertex);
            const UploadSlice up = Upload(bytes, 16);
            if (!up.Cpu) { S.Batch.clear(); return; }
            std::memcpy(up.Cpu, S.Batch.data(), bytes);
            const D3D12_VERTEX_BUFFER_VIEW vb{ up.Gpu, (UINT)bytes, (UINT)sizeof(Vertex) };
            S.List->IASetVertexBuffers(0, 1, &vb);
            if (S.BoundPso != S.BatchPso) { S.List->SetPipelineState(S.Psos[S.BatchPso].Get()); S.BoundPso = S.BatchPso; }
            if (!S.Bindless && S.BoundSrv != S.BatchSrv) { S.List->SetGraphicsRootDescriptorTable(1, SrvGpu(S.BatchSrv)); S.BoundSrv = S.BatchSrv; }
            S.List->DrawInstanced((UINT)S.Batch.size(), 1, 0, 0);
            S.Batch.clear();
        }
        // GPU に今までの命令を渡して終わるまで待ち、同じフレームの記録を続ける (読み戻し・大量の読み込み)
        void Restart() {
            if (!S.Recording) { return; }
            Flush();
            S.List->Close();
            ID3D12CommandList* lists[] = { S.List.Get() };
            S.Queue->ExecuteCommandLists(1, lists);
            S.Queue->Signal(S.Fence.Get(), ++S.FenceValue);
            S.Frames[S.FrameIndex].Fence = S.FenceValue;
            WaitFence(S.FenceValue);
            S.Recording = false;
            Begin();
        }
        Vertex* Reserve(size_t n, TextureObject* tex, int pso) {
            Begin();
            if (!S.Recording || !tex || tex->Srv == UINT32_MAX) { return nullptr; }
            if (tex == TargetObject()) { return nullptr; }     // 描画先そのものは読めない
            if (tex->State != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) { Flush(); Transition(tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); }
            if (S.BatchPso != pso || (!S.Bindless && S.BatchSrv != tex->Srv) || S.Batch.size() + n > MaxBatchVertices) { Flush(); }
            S.BatchPso = pso;
            S.BatchSrv = tex->Srv;
            const size_t old = S.Batch.size();
            S.Batch.resize(old + n);
            return S.Batch.data() + old;
        }
        // 4 隅 (左上・右上・右下・左下) と画像の範囲 (UV) で四角形を 2 つの三角形にして積む
        void EmitQuad(TextureObject* tex, int pso, const float* xy, float u0, float v0, float u1, float v1, uint32_t color, uint32_t add) {
            Vertex* v = Reserve(6, tex, pso);
            if (!v) { return; }
            const uint32_t ti = S.Bindless ? tex->Srv : 0;
            const Vertex a{ xy[0], xy[1], u0, v0, color, add, ti }, b{ xy[2], xy[3], u1, v0, color, add, ti };
            const Vertex c{ xy[4], xy[5], u1, v1, color, add, ti }, d{ xy[6], xy[7], u0, v1, color, add, ti };
            v[0] = a; v[1] = b; v[2] = c; v[3] = a; v[4] = c; v[5] = d;
        }

        // ---- 図形 (SDL 版と同じ形: 外側 1px を透明へ薄くしてアンチエイリアスに近づける) ----
        uint32_t PrimColor(Color c, float alpha) {
            return Pack(R(c) / 255.0f * S.BrightR / 255.0f, G(c) / 255.0f * S.BrightG / 255.0f, B(c) / 255.0f * S.BrightB / 255.0f, alpha);
        }
        int AddVertex(float x, float y, uint32_t c) { PrimVerts.push_back({ x, y, c }); return (int)PrimVerts.size() - 1; }
        void AddTri(int a, int b, int c) { PrimIndices.push_back(a); PrimIndices.push_back(b); PrimIndices.push_back(c); }
        void FlushPrim() {
            if (!PrimIndices.empty() && S.White) {
                TextureObject* white = S.White.Tex.get();
                if (Vertex* v = Reserve(PrimIndices.size(), white, PsoFor(S.Mode))) {
                    const uint32_t ti = S.Bindless ? white->Srv : 0;
                    for (size_t i = 0; i < PrimIndices.size(); ++i) {
                        const PrimVertex& p = PrimVerts[(size_t)PrimIndices[i]];
                        v[i] = { p.X, p.Y, 0.5f, 0.5f, p.C, 0, ti };
                    }
                }
            }
            PrimVerts.clear();
            PrimIndices.clear();
        }
        void Strip(float x1, float y1, float x2, float y2, float thickness, uint32_t c) {
            const float dx = x2 - x1, dy = y2 - y1, len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.0001f) { return; }
            const float nx = -dy / len, ny = dx / len, h = std::max(thickness, 1.0f) * 0.5f, f = h + 1.0f;
            const uint32_t edge = c & 0x00FFFFFFu;
            const int o0 = AddVertex(x1 + nx * f, y1 + ny * f, edge), i0 = AddVertex(x1 + nx * h, y1 + ny * h, c);
            const int i1 = AddVertex(x1 - nx * h, y1 - ny * h, c), o1 = AddVertex(x1 - nx * f, y1 - ny * f, edge);
            const int o2 = AddVertex(x2 + nx * f, y2 + ny * f, edge), i2 = AddVertex(x2 + nx * h, y2 + ny * h, c);
            const int i3 = AddVertex(x2 - nx * h, y2 - ny * h, c), o3 = AddVertex(x2 - nx * f, y2 - ny * f, edge);
            AddTri(o0, i0, o2); AddTri(i0, i2, o2);
            AddTri(i0, i1, i2); AddTri(i1, i3, i2);
            AddTri(i1, o1, i3); AddTri(o1, o3, i3);
        }

        void DrawTextureVerts(const Texture& t, const float* xy) {
            if (!t) { return; }
            TextureObject* o = t.Tex.get();
            if (o->W <= 0 || o->H <= 0) { return; }
            const float u0 = t.Src.x / o->W, v0 = t.Src.y / o->H, u1 = (t.Src.x + t.Src.w) / o->W, v1 = (t.Src.y + t.Src.h) / o->H;
            const uint32_t color = Pack(S.BrightR / 255.0f, S.BrightG / 255.0f, S.BrightB / 255.0f, Alpha());
            const uint32_t add = (uint32_t)S.AddR | ((uint32_t)S.AddG << 8) | ((uint32_t)S.AddB << 16);
            EmitQuad(o, PsoFor(S.Mode), xy, u0, v0, u1, v1, color, add);
        }

        // ---- 初期化 ----
        const char* ShaderSource = R"(
cbuffer Transform : register(b0) { float2 Scale; float2 Offset; };
struct VSIn { float2 Pos : POSITION; float2 Uv : TEXCOORD0; float4 Col : COLOR0; float4 Add : COLOR1; uint Tex : TEXCOORD1; };
struct PSIn { float4 Pos : SV_Position; float2 Uv : TEXCOORD0; float4 Col : COLOR0; float4 Add : COLOR1; nointerpolation uint Tex : TEXCOORD1; };
PSIn VS(VSIn i) {
    PSIn o;
    o.Pos = float4(i.Pos * Scale + Offset, 0, 1);
    o.Uv = i.Uv; o.Col = i.Col; o.Add = i.Add; o.Tex = i.Tex;
    return o;
}
SamplerState Smp : register(s0);
#ifdef BINDLESS
Texture2D Textures[] : register(t0);
#define SAMPLE(i, uv) Textures[NonUniformResourceIndex(i)].Sample(Smp, uv)
#else
Texture2D Texture0 : register(t0);
#define SAMPLE(i, uv) Texture0.Sample(Smp, uv)
#endif
float4 PS(PSIn i) : SV_Target {
    float4 t = SAMPLE(i.Tex, i.Uv);
    // DxLib の SetDrawBright / SetDrawAddColor: min(色 x 明るさ + 足す色, 1)
    return float4(min(t.rgb * i.Col.rgb + i.Add.rgb, 1.0), t.a * i.Col.a);
}
)";
        bool CreatePipelines() {
            const D3D_SHADER_MACRO defines[] = { { "BINDLESS", "1" }, { nullptr, nullptr } };
            ComPtr<ID3DBlob> vs, ps, err;
            const UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3 | (S.Bindless ? D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES : 0);
            HRESULT hr = D3DCompile(ShaderSource, std::strlen(ShaderSource), "fas", S.Bindless ? defines : nullptr, nullptr, "VS", "vs_5_1", flags, 0, &vs, &err);
            if (FAILED(hr)) { S.Error = err ? std::string((const char*)err->GetBufferPointer(), err->GetBufferSize()) : "shader"; return false; }
            hr = D3DCompile(ShaderSource, std::strlen(ShaderSource), "fas", S.Bindless ? defines : nullptr, nullptr, "PS", "ps_5_1", flags, 0, &ps, &err);
            if (FAILED(hr)) { S.Error = err ? std::string((const char*)err->GetBufferPointer(), err->GetBufferSize()) : "shader"; return false; }

            D3D12_DESCRIPTOR_RANGE range{};
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            range.NumDescriptors = S.Bindless ? UINT_MAX : 1;
            range.BaseShaderRegister = 0;
            range.OffsetInDescriptorsFromTableStart = 0;
            D3D12_ROOT_PARAMETER params[2]{};
            params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            params[0].Constants = { 0, 0, 4 };
            params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
            params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[1].DescriptorTable = { 1, &range };
            params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            D3D12_STATIC_SAMPLER_DESC sampler{};
            sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.MaxLOD = D3D12_FLOAT32_MAX;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            D3D12_ROOT_SIGNATURE_DESC rs{};
            rs.NumParameters = 2; rs.pParameters = params;
            rs.NumStaticSamplers = 1; rs.pStaticSamplers = &sampler;
            rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
            ComPtr<ID3DBlob> blob;
            if (FAILED(hr = D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) { return Fail("D3D12SerializeRootSignature", hr); }
            if (FAILED(hr = S.Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&S.Root)))) { return Fail("CreateRootSignature", hr); }

            const D3D12_INPUT_ELEMENT_DESC layout[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};
            pd.pRootSignature = S.Root.Get();
            pd.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
            pd.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
            pd.SampleMask = UINT_MAX;
            pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            pd.RasterizerState.DepthClipEnable = TRUE;
            pd.InputLayout = { layout, (UINT)std::size(layout) };
            pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            pd.NumRenderTargets = 1;
            pd.RTVFormats[0] = Format;
            pd.SampleDesc.Count = 1;
            // 合成は SDL 版と同じ (アルファは乗算しない値として扱う)
            struct BlendDef { bool On; D3D12_BLEND Src, Dst; D3D12_BLEND_OP Op; D3D12_BLEND SrcA, DstA; };
            const BlendDef defs[PsoCount] = {
                { false, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_ZERO },
                { true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA },
                { true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD, D3D12_BLEND_ZERO, D3D12_BLEND_ONE },
                { true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE, D3D12_BLEND_OP_REV_SUBTRACT, D3D12_BLEND_ZERO, D3D12_BLEND_ONE },
                { true, D3D12_BLEND_DEST_COLOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ZERO, D3D12_BLEND_ONE },
            };
            for (int i = 0; i < PsoCount; ++i) {
                auto& rt = pd.BlendState.RenderTarget[0];
                rt = {};
                rt.BlendEnable = defs[i].On;
                rt.SrcBlend = defs[i].Src; rt.DestBlend = defs[i].Dst; rt.BlendOp = defs[i].Op;
                rt.SrcBlendAlpha = defs[i].SrcA; rt.DestBlendAlpha = defs[i].DstA; rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
                rt.LogicOp = D3D12_LOGIC_OP_NOOP;
                rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
                if (FAILED(hr = S.Device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&S.Psos[i])))) { return Fail("CreateGraphicsPipelineState", hr); }
            }
            return true;
        }
        bool AcquireBackBuffers() {
            for (UINT i = 0; i < BackBufferCount; ++i) {
                if (FAILED(S.Swap->GetBuffer(i, IID_PPV_ARGS(&S.Back[i])))) { return false; }
                S.Device->CreateRenderTargetView(S.Back[i].Get(), nullptr, RtvCpu(S.BackRtv[i]));
            }
            DXGI_SWAP_CHAIN_DESC1 d{};
            S.Swap->GetDesc1(&d);
            S.BackW = (int)d.Width; S.BackH = (int)d.Height;
            return true;
        }
        UINT SwapFlags() { return DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | (S.Tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0); }
        void ResizeIfNeeded() {
            if (!S.ResizePending) { return; }
            int w = 0, h = 0;
            win::ClientSize(w, h);
            if (w <= 0 || h <= 0) { return; }       // 最小化中は後で
            S.ResizePending = false;
            if (w == S.BackW && h == S.BackH) { return; }
            WaitIdle();
            for (auto& b : S.Back) { b.Reset(); }
            if (FAILED(S.Swap->ResizeBuffers(BackBufferCount, (UINT)w, (UINT)h, Format, SwapFlags()))) { S.BackW = S.BackH = 0; return; }
            AcquireBackBuffers();
        }
        // ウィンドウの中で画面用テクスチャを表示する位置 (縦横比を保って中央に)
        void PresentRect(float ow, float oh, float& x, float& y, float& w, float& h) {
            const float scale = std::min(ow / S.LogicalW, oh / S.LogicalH);
            w = S.LogicalW * scale; h = S.LogicalH * scale;
            x = (ow - w) * 0.5f; y = (oh - h) * 0.5f;
        }
        Texture MakeTexture(int width, int height, bool target) {
            Texture t;
            if (!S.Alive || width <= 0 || height <= 0) { return t; }
            auto o = std::make_shared<TextureObject>();
            D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_DEFAULT };
            D3D12_RESOURCE_DESC rd{};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            rd.Width = (UINT64)width; rd.Height = (UINT)height; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
            rd.Format = Format; rd.SampleDesc.Count = 1;
            rd.Flags = target ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;
            D3D12_CLEAR_VALUE clear{ Format, { 0, 0, 0, 1 } };
            o->State = target ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COPY_DEST;
            HRESULT hr = S.Device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, o->State, target ? &clear : nullptr, IID_PPV_ARGS(&o->Res));
            if (FAILED(hr)) { Fail("CreateCommittedResource (texture)", hr); return t; }
            o->W = width; o->H = height;
            o->Srv = S.SrvPool.Alloc();
            if (o->Srv == UINT32_MAX) { S.Error = "too many textures"; return t; }
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
            sd.Format = Format;
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sd.Texture2D.MipLevels = 1;
            S.Device->CreateShaderResourceView(o->Res.Get(), &sd, SrvCpu(o->Srv));
            if (target) {
                o->Rtv = S.RtvPool.Alloc();
                if (o->Rtv == UINT32_MAX) { S.Error = "too many render targets"; return t; }
                S.Device->CreateRenderTargetView(o->Res.Get(), nullptr, RtvCpu(o->Rtv));
                // 新しい描画先は透明にしておく
                Begin();
                Flush();
                const float zero[4] = { 0, 0, 0, 0 };
                S.List->ClearRenderTargetView(RtvCpu(o->Rtv), zero, 0, nullptr);
            }
            t.Tex = std::move(o);
            t.Src = { 0, 0, (float)width, (float)height };
            return t;
        }
        Texture FromImage(const Image& img) {
            if (!img) { return {}; }
            Texture t = MakeTexture(img.W, img.H, false);
            if (t) { UpdateTexture(t, img.Pixels.data(), img.W * 4); }
            return t;
        }
    }

    TextureObject::~TextureObject() {
        // GPU が使い終わるまで (今記録しているフレームが終わるまで) 解放を待つ
        if (S.Alive) { S.Trash.push_back({ S.FenceValue + 1, std::move(Res), Srv, Rtv }); }
    }

    bool Init(const InitOptions& o) {
        if (S.Alive) { return true; }
        S.Error.clear();
        win::WindowOptions wo;
        wo.Title = win::Widen(o.Title);
        wo.ClientW = o.Width; wo.ClientH = o.Height;
        wo.FullScreen = o.FullScreen; wo.Resizable = o.Resizable; wo.Hidden = o.Hidden;
        wo.IconResource = o.IconResource;
        S.Hwnd = win::CreateMainWindow(wo);
        if (!S.Hwnd) { S.Error = "CreateWindow failed"; return false; }

        HRESULT hr;
        UINT factoryFlags = 0;
        char debugEnv[8]{};
        if (GetEnvironmentVariableA("FAS_D3D12_DEBUG", debugEnv, sizeof(debugEnv)) > 0 && debugEnv[0] == '1') {
            ComPtr<ID3D12Debug> debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) { debug->EnableDebugLayer(); factoryFlags = DXGI_CREATE_FACTORY_DEBUG; }
        }
        if (FAILED(hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&S.Factory)))) { return Fail("CreateDXGIFactory2", hr); }
        // Windows の「グラフィックの設定」で選んだ GPU (既定のアダプター) を使う
        if (FAILED(hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&S.Device)))) {
            ComPtr<IDXGIAdapter> warp;
            if (FAILED(S.Factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) || FAILED(hr = D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&S.Device)))) {
                return Fail("D3D12CreateDevice", hr);
            }
        }
        if (factoryFlags) { S.Device.As(&S.Debug); }
        {
            ComPtr<IDXGIFactory5> f5;
            BOOL tearing = FALSE;
            if (SUCCEEDED(S.Factory.As(&f5)) && SUCCEEDED(f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing)))) { S.Tearing = tearing != FALSE; }
            D3D12_FEATURE_DATA_D3D12_OPTIONS opt{};
            if (SUCCEEDED(S.Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opt, sizeof(opt)))) { S.Bindless = opt.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_2; }
        }
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        qd.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
        if (FAILED(hr = S.Device->CreateCommandQueue(&qd, IID_PPV_ARGS(&S.Queue)))) {
            qd.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            if (FAILED(hr = S.Device->CreateCommandQueue(&qd, IID_PPV_ARGS(&S.Queue)))) { return Fail("CreateCommandQueue", hr); }
        }
        for (auto& f : S.Frames) {
            if (FAILED(hr = S.Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&f.Alloc)))) { return Fail("CreateCommandAllocator", hr); }
        }
        if (FAILED(hr = S.Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, S.Frames[0].Alloc.Get(), nullptr, IID_PPV_ARGS(&S.List)))) { return Fail("CreateCommandList", hr); }
        S.List->Close();
        if (FAILED(hr = S.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&S.Fence)))) { return Fail("CreateFence", hr); }
        S.FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors = SrvCapacity;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(hr = S.Device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&S.SrvHeap)))) { return Fail("CreateDescriptorHeap (SRV)", hr); }
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = RtvCapacity;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(hr = S.Device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&S.RtvHeap)))) { return Fail("CreateDescriptorHeap (RTV)", hr); }
        S.SrvInc = S.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        S.RtvInc = S.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        S.SrvPool = { {}, 0, SrvCapacity };
        S.RtvPool = { {}, 0, RtvCapacity };
        for (auto& r : S.BackRtv) { r = S.RtvPool.Alloc(); }
        // 使われない記述子も、バインドレスの範囲の中では正しい内容にしておく (1×1 の黒)
        if (!CreatePipelines()) { return false; }

        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Format = Format;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = BackBufferCount;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        sd.Flags = SwapFlags();
        ComPtr<IDXGISwapChain1> swap1;
        if (FAILED(hr = S.Factory->CreateSwapChainForHwnd(S.Queue.Get(), S.Hwnd, &sd, nullptr, nullptr, &swap1))) { return Fail("CreateSwapChainForHwnd", hr); }
        if (FAILED(hr = swap1.As(&S.Swap))) { return Fail("IDXGISwapChain3", hr); }
        S.Factory->MakeWindowAssociation(S.Hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
        // 表示待ちの列を 1 フレームにして、入力から表示までを短くする
        S.Swap->SetMaximumFrameLatency(1);
        S.Waitable = S.Swap->GetFrameLatencyWaitableObject();
        if (!AcquireBackBuffers()) { return Fail("GetBuffer", E_FAIL); }

        S.Alive = true;
        S.VSync = o.VSync;
        S.Batch.reserve(MaxBatchVertices);
        S.White = MakeTexture(1, 1, false);
        const uint32_t white = 0xFFFFFFFFu;
        UpdateTexture(S.White, &white, 4);
        S.Canvas = {};
        SetLogicalSize(o.Width, o.Height);
        if (S.Waitable) { WaitForSingleObjectEx(S.Waitable, 1000, TRUE); }
        return true;
    }
    void Shutdown() {
        if (!S.Alive) { win::DestroyMainWindow(); return; }
        if (S.Recording) { S.List->Close(); S.Recording = false; }
        WaitIdle();
        S.Batch.clear();
        S.Canvas = {};
        S.White = {};
        S.Target = nullptr;
        S.Alive = false;        // これ以降に消えるテクスチャはすぐ解放する
        S.Trash.clear();
        for (auto& f : S.Frames) { f.Pages.clear(); f.Big.clear(); f.Alloc.Reset(); f.Fence = 0; }
        S.Readback.Reset(); S.ReadbackSize = 0;
        S.ReadSlots.clear();
        for (auto& b : S.Back) { b.Reset(); }
        if (S.Waitable) { CloseHandle(S.Waitable); S.Waitable = nullptr; }
        S.Swap.Reset();
        for (auto& p : S.Psos) { p.Reset(); }
        S.Root.Reset(); S.List.Reset(); S.SrvHeap.Reset(); S.RtvHeap.Reset();
        S.Fence.Reset();
        if (S.FenceEvent) { CloseHandle(S.FenceEvent); S.FenceEvent = nullptr; }
        S.Queue.Reset(); S.Device.Reset(); S.Factory.Reset();
        S.FenceValue = 0;
        win::DestroyMainWindow();
        S.Hwnd = nullptr;
    }
    bool IsInit() { return S.Alive; }
    std::string LastError() { return S.Error; }
    const char* BackendName() { return S.Bindless ? "Direct3D12" : "Direct3D12 (bound)"; }
    int RefreshRate() {
        if (!S.Hwnd) { return 0; }
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(MonitorFromWindow(S.Hwnd, MONITOR_DEFAULTTONEAREST), &mi)) { return 0; }
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) || dm.dmDisplayFrequency <= 1) { return 0; }
        return (int)dm.dmDisplayFrequency;
    }
    void RaiseWindow() {
        if (!S.Hwnd) { return; }
        ShowWindow(S.Hwnd, SW_SHOW);
        // 他のアプリが前面の時は、入力を一時的につないで前面に出す
        const HWND fg = GetForegroundWindow();
        const DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0, me = GetCurrentThreadId();
        if (fgThread && fgThread != me) { AttachThreadInput(fgThread, me, TRUE); }
        SetForegroundWindow(S.Hwnd);
        BringWindowToTop(S.Hwnd);
        SetFocus(S.Hwnd);
        if (fgThread && fgThread != me) { AttachThreadInput(fgThread, me, FALSE); }
    }

    void SetLogicalSize(int width, int height) {
        width = std::max(width, 1);
        height = std::max(height, 1);
        if (S.Canvas && width == S.LogicalW && height == S.LogicalH) { return; }
        S.LogicalW = width;
        S.LogicalH = height;
        if (!S.Alive) { return; }
        if (S.Recording) { Flush(); }
        S.Canvas = MakeTexture(width, height, true);
        if (S.Recording && !S.Target) { BindTarget(); }
    }
    int LogicalWidth() { return S.LogicalW; }
    int LogicalHeight() { return S.LogicalH; }
    void SetVSync(bool on) { S.VSync = on; }
    void SetFullScreen(bool on) { win::SetFullScreen(on); }
    bool IsFullScreen() { return win::IsFullScreen(); }
    void SetWindowVisible(bool visible) { if (S.Hwnd) { ShowWindow(S.Hwnd, visible ? SW_SHOW : SW_HIDE); } }
    void SetWindowTitle(const char* title) { if (S.Hwnd) { SetWindowTextW(S.Hwnd, win::Widen(title ? title : "").c_str()); } }
    void WindowToLogical(float wx, float wy, float& lx, float& ly) {
        int ow = 0, oh = 0;
        win::ClientSize(ow, oh);
        lx = wx; ly = wy;
        if (ow <= 0 || oh <= 0) { return; }
        float x, y, w, h;
        PresentRect((float)ow, (float)oh, x, y, w, h);
        lx = w > 0 ? (wx - x) * S.LogicalW / w : wx;
        ly = h > 0 ? (wy - y) * S.LogicalH / h : wy;
    }
    void LogicalToWindow(float lx, float ly, float& wx, float& wy) {
        int ow = 0, oh = 0;
        win::ClientSize(ow, oh);
        wx = lx; wy = ly;
        if (ow <= 0 || oh <= 0) { return; }
        float x, y, w, h;
        PresentRect((float)ow, (float)oh, x, y, w, h);
        wx = x + lx * w / S.LogicalW;
        wy = y + ly * h / S.LogicalH;
    }

    void Clear(Color color, int alpha) {
        Begin();
        if (!S.Recording) { return; }
        Flush();
        TextureObject* t = TargetObject();
        if (!t) { return; }
        const float c[4] = { R(color) / 255.0f, G(color) / 255.0f, B(color) / 255.0f, std::clamp(alpha, 0, 255) / 255.0f };
        S.List->ClearRenderTargetView(RtvCpu(t->Rtv), c, 0, nullptr);
    }
    void Present() {
        Begin();
        if (!S.Recording) { return; }
        Flush();
        ResizeIfNeeded();
        const bool show = S.Back[0] && S.BackW > 0 && S.BackH > 0 && !IsIconic(S.Hwnd);
        if (show && S.Canvas) {
            // 画面用テクスチャをバックバッファに縦横比を保って拡大して写す (余白は黒)
            const UINT bi = S.Swap->GetCurrentBackBufferIndex();
            ID3D12Resource* back = S.Back[bi].Get();
            BackBarrier(back, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            const auto rtv = RtvCpu(S.BackRtv[bi]);
            S.List->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            const float black[4] = { 0, 0, 0, 1 };
            S.List->ClearRenderTargetView(rtv, black, 0, nullptr);
            SetViewport(S.BackW, S.BackH);
            const D3D12_RECT full{ 0, 0, S.BackW, S.BackH };
            S.List->RSSetScissorRects(1, &full);
            float x, y, w, h;
            PresentRect((float)S.BackW, (float)S.BackH, x, y, w, h);
            TextureObject* canvas = S.Canvas.Tex.get();
            Transition(canvas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            const float xy[8] = { x, y, x + w, y, x + w, y + h, x, y + h };
            // 描画先の判定を避けるため、ここだけは直接積む
            const uint32_t ti = S.Bindless ? canvas->Srv : 0;
            if (S.BatchPso != PsoOpaque || (!S.Bindless && S.BatchSrv != canvas->Srv)) { Flush(); }
            S.BatchPso = PsoOpaque; S.BatchSrv = canvas->Srv;
            const Vertex a{ xy[0], xy[1], 0, 0, 0xFFFFFFFFu, 0, ti }, b{ xy[2], xy[3], 1, 0, 0xFFFFFFFFu, 0, ti };
            const Vertex c{ xy[4], xy[5], 1, 1, 0xFFFFFFFFu, 0, ti }, d{ xy[6], xy[7], 0, 1, 0xFFFFFFFFu, 0, ti };
            S.Batch.insert(S.Batch.end(), { a, b, c, a, c, d });
            Flush();
            BackBarrier(back, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        }
        S.List->Close();
        ID3D12CommandList* lists[] = { S.List.Get() };
        S.Queue->ExecuteCommandLists(1, lists);
        S.Recording = false;
        HRESULT hr = S_OK;
        if (show) {
            const bool tear = !S.VSync && S.Tearing;
            hr = S.Swap->Present(S.VSync ? 1 : 0, tear ? DXGI_PRESENT_ALLOW_TEARING : 0);
        }
        S.Queue->Signal(S.Fence.Get(), ++S.FenceValue);
        S.Frames[S.FrameIndex].Fence = S.FenceValue;
        S.FrameIndex = (S.FrameIndex + 1) % FrameCount;
        DumpDebugMessages();
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            Fail("GPU device removed", S.Device->GetDeviceRemovedReason());
            PostMessageW(S.Hwnd, WM_CLOSE, 0, 0);
            return;
        }
        // 次のフレームを描き始めてよくなるまで待つ (表示待ちの列が 1 フレームを超えないように)
        if (show && S.Waitable) { WaitForSingleObjectEx(S.Waitable, 100, TRUE); }
        else if (!show) { Sleep(8); }
    }

    void SetBlend(Blend mode, int param) { S.Mode = mode; S.Param = std::clamp(param, 0, 255); }
    Blend CurrentBlend() { return S.Mode; }
    int CurrentBlendParam() { return S.Param; }
    void SetClip(int left, int top, int right, int bottom) {
        const D3D12_RECT r{ left, top, std::max(right, left), std::max(bottom, top) };
        if (S.ClipOn && std::memcmp(&r, &S.Clip, sizeof(r)) == 0) { return; }
        if (S.Recording) { Flush(); }
        S.ClipOn = true;
        S.Clip = r;
        if (S.Recording) { ApplyScissor(); }
    }
    void ResetClip() {
        if (!S.ClipOn) { return; }
        if (S.Recording) { Flush(); }
        S.ClipOn = false;
        if (S.Recording) { ApplyScissor(); }
    }
    bool GetClip(int& left, int& top, int& right, int& bottom) {
        left = (int)S.Clip.left; top = (int)S.Clip.top; right = (int)S.Clip.right; bottom = (int)S.Clip.bottom;
        return S.ClipOn;
    }
    void SetBright(int r, int g, int b) { S.BrightR = std::clamp(r, 0, 255); S.BrightG = std::clamp(g, 0, 255); S.BrightB = std::clamp(b, 0, 255); }
    void GetBright(int& r, int& g, int& b) { r = S.BrightR; g = S.BrightG; b = S.BrightB; }
    void SetAddColor(int r, int g, int b) { S.AddR = std::clamp(r, 0, 255); S.AddG = std::clamp(g, 0, 255); S.AddB = std::clamp(b, 0, 255); }

    void Line(float x1, float y1, float x2, float y2, Color color, float thickness) {
        Strip(x1, y1, x2, y2, thickness, PrimColor(color, Alpha()));
        FlushPrim();
    }
    void Box(float l, float t, float r, float b, Color color, bool fill, float thickness) {
        if (r < l) { std::swap(l, r); }
        if (b < t) { std::swap(t, b); }
        const uint32_t c = PrimColor(color, Alpha());
        if (fill) {
            const int a = AddVertex(l, t, c), b1 = AddVertex(r, t, c), c1 = AddVertex(r, b, c), d = AddVertex(l, b, c);
            AddTri(a, b1, c1); AddTri(a, c1, d);
        }
        else {
            const float h = thickness * 0.5f;
            Strip(l - h, t, r + h, t, thickness, c); Strip(r, t, r, b, thickness, c);
            Strip(r + h, b, l - h, b, thickness, c); Strip(l, b, l, t, thickness, c);
        }
        FlushPrim();
    }
    void Oval(float cx, float cy, float rx, float ry, int segments, Color color, bool fill, float thickness) {
        segments = std::clamp(segments, 8, 256);
        const uint32_t c = PrimColor(color, Alpha());
        const uint32_t edge = c & 0x00FFFFFFu;
        constexpr float pi2 = 6.28318530718f;
        if (fill) {
            const int center = AddVertex(cx, cy, c);
            int prevIn = -1, prevOut = -1, firstIn = -1, firstOut = -1;
            for (int i = 0; i <= segments; ++i) {
                const float a = pi2 * i / segments, ca = std::cos(a), sa = std::sin(a);
                const int in = i == segments ? firstIn : AddVertex(cx + ca * rx, cy + sa * ry, c);
                const int out = i == segments ? firstOut : AddVertex(cx + ca * (rx + 1), cy + sa * (ry + 1), edge);
                if (i == 0) { firstIn = in; firstOut = out; }
                else { AddTri(center, prevIn, in); AddTri(prevIn, prevOut, out); AddTri(prevIn, out, in); }
                prevIn = in; prevOut = out;
            }
        }
        else {
            for (int i = 0; i < segments; ++i) {
                const float a0 = pi2 * i / segments, a1 = pi2 * (i + 1) / segments;
                Strip(cx + std::cos(a0) * rx, cy + std::sin(a0) * ry, cx + std::cos(a1) * rx, cy + std::sin(a1) * ry, thickness, c);
            }
        }
        FlushPrim();
    }
    void Circle(float cx, float cy, float r, int segments, Color color, bool fill, float thickness) { Oval(cx, cy, r, r, segments, color, fill, thickness); }
    void Triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color, bool fill) {
        const uint32_t c = PrimColor(color, Alpha());
        if (fill) { AddTri(AddVertex(x1, y1, c), AddVertex(x2, y2, c), AddVertex(x3, y3, c)); }
        else { Strip(x1, y1, x2, y2, 1, c); Strip(x2, y2, x3, y3, 1, c); Strip(x3, y3, x1, y1, 1, c); }
        FlushPrim();
    }
    void Pixel(float x, float y, Color color) { Box(x, y, x + 1, y + 1, color, true); }

    Texture LoadTexture(const std::string& path) {
        Image img;
        if (!win::DecodeImageFile(path, img)) { return {}; }
        return FromImage(img);
    }
    Texture LoadTextureFromMemory(const void* data, size_t size) {
        Image img;
        if (!win::DecodeImageMemory(data, size, img)) { return {}; }
        return FromImage(img);
    }
    std::vector<Texture> Divide(const Texture& source, int nx, int ny, float cw, float ch) {
        std::vector<Texture> out;
        if (!source || nx <= 0 || ny <= 0) { return out; }
        out.reserve((size_t)nx * ny);
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) { out.push_back(SubTexture(source, source.Src.x + cw * x, source.Src.y + ch * y, cw, ch)); }
        }
        return out;
    }
    std::vector<Texture> LoadDivTexture(const std::string& path, int nx, int ny, float cw, float ch) { return Divide(LoadTexture(path), nx, ny, cw, ch); }
    Texture SubTexture(const Texture& source, float x, float y, float w, float h) {
        Texture t = source;
        t.Src = { x, y, w, h };
        return t;
    }
    bool ImageSize(const std::string& path, int& width, int& height) { return win::ImageFileSize(path, width, height); }
    std::vector<uint32_t> LoadPixels(const std::string& path, int& width, int& height) {
        Image img;
        width = height = 0;
        if (!win::DecodeImageFile(path, img)) { return {}; }
        width = img.W; height = img.H;
        return std::move(img.Pixels);
    }
    Texture CreateTexture(int width, int height, bool target) { return MakeTexture(width, height, target); }
    void UpdateTexture(const Texture& texture, const void* pixels, int pitch) {
        if (!texture || !pixels) { return; }
        Begin();
        if (!S.Recording) { return; }
        Flush();
        TextureObject* o = texture.Tex.get();
        const size_t row = (size_t)o->W * 4, rowPitch = AlignUp(row, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        const UploadSlice up = Upload(rowPitch * o->H, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (!up.Cpu) { return; }
        const uint8_t* src = (const uint8_t*)pixels;
        if (rowPitch == row && (size_t)pitch == row) { std::memcpy(up.Cpu, src, row * o->H); }
        else { for (int y = 0; y < o->H; ++y) { std::memcpy(up.Cpu + rowPitch * y, src + (size_t)pitch * y, row); } }
        Transition(o, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION dst{ o->Res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
        dst.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION from{ up.Res, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
        from.PlacedFootprint.Offset = up.Offset;
        from.PlacedFootprint.Footprint = { Format, (UINT)o->W, (UINT)o->H, 1, (UINT)rowPitch };
        S.List->CopyTextureRegion(&dst, 0, 0, 0, &from, nullptr);
        Transition(o, o == TargetObject() ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        if (S.FrameUploadBytes >= RestartUploadBytes) { Restart(); }
    }

    void DrawTexture(const Texture& t, float l, float tp, float r, float b) {
        const float xy[8] = { l, tp, r, tp, r, b, l, b };
        DrawTextureVerts(t, xy);
    }
    void DrawTextureRect(const Texture& t, float x, float y, float sx, float sy, float sw, float sh) {
        if (!t) { return; }
        DrawTexture(SubTexture(t, t.Src.x + sx, t.Src.y + sy, sw, sh), x, y, x + sw, y + sh);
    }
    void DrawTexturePart(const Texture& t, float l, float tp, float r, float b, float sx, float sy, float sw, float sh) {
        if (!t) { return; }
        DrawTexture(SubTexture(t, t.Src.x + sx, t.Src.y + sy, sw, sh), l, tp, r, b);
    }
    void DrawTextureQuad(const Texture& t, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {
        const float xy[8] = { x1, y1, x2, y2, x3, y3, x4, y4 };
        DrawTextureVerts(t, xy);
    }
    void DrawTextureRotated(const Texture& t, float cx, float cy, float scale, float angle, bool flipx) {
        if (!t) { return; }
        const float hw = t.Src.w * scale * 0.5f * (flipx ? -1.0f : 1.0f), hh = t.Src.h * scale * 0.5f;
        const float c = std::cos(angle), s = std::sin(angle);
        auto px = [&](float x, float y) { return cx + x * c - y * s; };
        auto py = [&](float x, float y) { return cy + x * s + y * c; };
        DrawTextureQuad(t, px(-hw, -hh), py(-hw, -hh), px(hw, -hh), py(hw, -hh), px(hw, hh), py(hw, hh), px(-hw, hh), py(-hw, hh));
    }

    void SetTarget(const Texture* texture) {
        if (texture && (!*texture || texture->Tex->Rtv == UINT32_MAX)) { return; }
        if (S.Recording) { Flush(); }
        S.Target = texture;
        if (S.Recording) { BindTarget(); }
    }
    Image ReadPixels(int x, int y, int w, int h) {
        Image out;
        Begin();
        if (!S.Recording) { return out; }
        TextureObject* t = TargetObject();
        if (!t) { return out; }
        x = std::clamp(x, 0, t->W); y = std::clamp(y, 0, t->H);
        w = std::clamp(w, 0, t->W - x); h = std::clamp(h, 0, t->H - y);
        if (w <= 0 || h <= 0) { return out; }
        Flush();
        const size_t rowPitch = AlignUp((size_t)w * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT), bytes = rowPitch * h;
        if (!S.Readback || S.ReadbackSize < bytes) {
            S.Readback.Reset();
            D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_READBACK };
            D3D12_RESOURCE_DESC rd{};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = bytes; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
            rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if (FAILED(S.Device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&S.Readback)))) { S.ReadbackSize = 0; return out; }
            S.ReadbackSize = bytes;
        }
        Transition(t, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION dst{ S.Readback.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
        dst.PlacedFootprint.Offset = 0;
        dst.PlacedFootprint.Footprint = { Format, (UINT)w, (UINT)h, 1, (UINT)rowPitch };
        D3D12_TEXTURE_COPY_LOCATION src{ t->Res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
        src.SubresourceIndex = 0;
        const D3D12_BOX box{ (UINT)x, (UINT)y, 0, (UINT)(x + w), (UINT)(y + h), 1 };
        S.List->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
        Transition(t, D3D12_RESOURCE_STATE_RENDER_TARGET);
        Restart();
        void* p = nullptr;
        const D3D12_RANGE range{ 0, bytes };
        if (FAILED(S.Readback->Map(0, &range, &p)) || !p) { return out; }
        out.W = w; out.H = h;
        out.Pixels.resize((size_t)w * h);
        for (int row = 0; row < h; ++row) { std::memcpy(out.Pixels.data() + (size_t)row * w, (const uint8_t*)p + rowPitch * row, (size_t)w * 4); }
        const D3D12_RANGE none{ 0, 0 };
        S.Readback->Unmap(0, &none);
        return out;
    }
    bool ReadPixelsAsync(int x, int y, int w, int h) {
        Begin();
        if (!S.Recording) { return false; }
        TextureObject* t = TargetObject();
        if (!t) { return false; }
        x = std::clamp(x, 0, t->W); y = std::clamp(y, 0, t->H);
        w = std::clamp(w, 0, t->W - x); h = std::clamp(h, 0, t->H - y);
        if (w <= 0 || h <= 0) { return false; }
        State::ReadSlot* slot = nullptr;
        for (auto& s : S.ReadSlots) { if (!s.Pending) { slot = &s; break; } }
        if (!slot) {
            if (S.ReadSlots.size() >= 4) { return false; }     // GPU が大きく遅れている: このフレームは諦める
            slot = &S.ReadSlots.emplace_back();
        }
        const size_t pitch = AlignUp((size_t)w * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT), bytes = pitch * h;
        if (!slot->Res || slot->Size < bytes) {
            slot->Res.Reset();
            D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_READBACK };
            D3D12_RESOURCE_DESC rd{};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = bytes; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
            rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if (FAILED(S.Device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&slot->Res)))) { slot->Size = 0; return false; }
            slot->Size = bytes;
        }
        Flush();
        Transition(t, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION dst{ slot->Res.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
        dst.PlacedFootprint.Offset = 0;
        dst.PlacedFootprint.Footprint = { Format, (UINT)w, (UINT)h, 1, (UINT)pitch };
        D3D12_TEXTURE_COPY_LOCATION src{ t->Res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
        src.SubresourceIndex = 0;
        const D3D12_BOX box{ (UINT)x, (UINT)y, 0, (UINT)(x + w), (UINT)(y + h), 1 };
        S.List->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
        Transition(t, D3D12_RESOURCE_STATE_RENDER_TARGET);
        slot->Fence = S.FenceValue + 1;     // このフレームを GPU に渡した時の値
        slot->Order = ++S.ReadOrder;
        slot->W = w; slot->H = h; slot->Pitch = pitch;
        slot->Pending = true;
        return true;
    }
    bool TakeReadPixels(Image& out, bool wait) {
        State::ReadSlot* slot = nullptr;
        for (auto& s : S.ReadSlots) { if (s.Pending && (!slot || s.Order < slot->Order)) { slot = &s; } }
        if (!slot) { return false; }
        if (S.Fence->GetCompletedValue() < slot->Fence) {
            if (!wait) { return false; }
            if (S.Recording && slot->Fence > S.FenceValue) { Restart(); }     // まだ GPU に渡していない
            WaitFence(slot->Fence);
        }
        void* p = nullptr;
        const D3D12_RANGE range{ 0, slot->Pitch * slot->H };
        slot->Pending = false;
        if (FAILED(slot->Res->Map(0, &range, &p)) || !p) { return false; }
        out.W = slot->W; out.H = slot->H;
        out.Pixels.resize((size_t)out.W * out.H);
        for (int row = 0; row < out.H; ++row) { std::memcpy(out.Pixels.data() + (size_t)row * out.W, (const uint8_t*)p + slot->Pitch * row, (size_t)out.W * 4); }
        const D3D12_RANGE none{ 0, 0 };
        slot->Res->Unmap(0, &none);
        return true;
    }
    bool SavePng(const std::string& path, int x, int y, int w, int h) {
        const Image img = ReadPixels(x, y, w, h);
        return img && win::EncodePng(path, img);
    }
    bool CaptureScreen(Texture& texture, int w, int h) {
        Begin();
        if (!S.Recording || w <= 0 || h <= 0) { return false; }
        TextureObject* src = TargetObject();
        if (!src) { return false; }
        if (texture.Tex.get() == src) { return true; }
        if (!texture || texture.Tex->W != w || texture.Tex->H != h) { texture = MakeTexture(w, h, false); }
        if (!texture) { return false; }
        Flush();
        TextureObject* dst = texture.Tex.get();
        Transition(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Transition(dst, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION d{ dst->Res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
        d.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION s{ src->Res.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
        s.SubresourceIndex = 0;
        const D3D12_BOX box{ 0, 0, 0, (UINT)std::min(w, src->W), (UINT)std::min(h, src->H), 1 };
        S.List->CopyTextureRegion(&d, 0, 0, 0, &s, &box);
        Transition(dst, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        Transition(src, D3D12_RESOURCE_STATE_RENDER_TARGET);
        return true;
    }
}

namespace win {
    void NotifyResize() { gfx::S.ResizePending = true; }
    void DrawTinted(const gfx::Texture& t, float l, float tp, float r, float b, gfx::Color color, int alpha) {
        using namespace gfx;
        if (!t) { return; }
        TextureObject* o = t.Tex.get();
        if (o->W <= 0 || o->H <= 0) { return; }
        const float xy[8] = { l, tp, r, tp, r, b, l, b };
        const float u0 = t.Src.x / o->W, v0 = t.Src.y / o->H, u1 = (t.Src.x + t.Src.w) / o->W, v1 = (t.Src.y + t.Src.h) / o->H;
        EmitQuad(o, PsoAlpha, xy, u0, v0, u1, v1, Pack(R(color) / 255.0f, G(color) / 255.0f, B(color) / 255.0f, std::clamp(alpha, 0, 255) / 255.0f), 0);
    }
}

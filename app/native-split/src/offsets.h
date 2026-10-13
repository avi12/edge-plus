// Edge split-view ABI, pinned to msedge.dll 155.0.4283.45 (image base 0x180000000).
// Single source of truth in code; the prose is docs/re/split-view-abi.md.
// Every value is a module-relative RVA or a struct field offset verified from the
// public PDB + disassembly. Re-verify after an Edge update (tools/re/pdbid.py).
#pragma once
#include <cstdint>

namespace fixedge::edge155 {

// The build these offsets belong to. DllMain refuses to hook anything else.
inline constexpr wchar_t kModule[] = L"msedge.dll";
inline constexpr char kBuild[] = "155.0.4283.45";
inline constexpr uint32_t kTimeDateStamp = 0;  // TODO: fill from PE header as an extra guard

// --- function RVAs (add to the msedge.dll base at runtime) ---
namespace rva {
inline constexpr uintptr_t MultiContentsView_ctor              = 0x2cdf896;
inline constexpr uintptr_t MultiContentsView_SetWebContentsAtIndex = 0xc930f18; // cap: index < 2
inline constexpr uintptr_t MultiContentsView_ShowSplitView      = 0xc930ffc;    // loops the vector
inline constexpr uintptr_t MultiContentsView_CalculateProposedLayout = 0x2b22540; // 2 entries + 1 sep
inline constexpr uintptr_t MultiContentsView_GetViewSizes       = 0x2b229e0;     // 2-element size array
inline constexpr uintptr_t MultiContentsView_CalculateSeparatorLayouts = 0x2b22ae6;
inline constexpr uintptr_t MultiContentsView_ClampToMinSize     = 0x2b22aa2;
inline constexpr uintptr_t MultiContentsView_ExecuteOnEachVisible = 0x2b56844;   // already N-ready
inline constexpr uintptr_t EdgeMultiContentsResizeArea_ctor     = 0x2ce11b0;
inline constexpr uintptr_t ContentsContainerView_ctor           = 0x2ce1b32;
inline constexpr uintptr_t EdgeContentsContainerView_ctor       = 0x2ce12b0;
inline constexpr uintptr_t SplitWindowControllerImplV2_ctor     = 0x2ccfac2;
inline constexpr uintptr_t IsEdgeSplitWindowEnabled             = 0x0fb9f2d;
// The layout applier + the one append primitive the geometry uses. emplace_back
// grows the real std::vector<ChildLayout> with the real allocator, so calling it
// is the only safe way to add pane/separator entries from the injected DLL.
inline constexpr uintptr_t LayoutManagerBase_ApplyLayout        = 0x0fd3ed8;
// std::vector<ChildLayout>::emplace_back<View*, bool, const gfx::Rect&>
//   (rcx = &vector, rdx = View**, r8 = bool*, r9 = gfx::Rect*)
inline constexpr uintptr_t ChildLayouts_emplace_back            = 0x278099a;
}  // namespace rva

// --- struct field offsets ---
namespace off {
// MultiContentsView : public views::View, public views::LayoutManagerBase.
// The View base is at +0; the LayoutManagerBase subobject is embedded at +0x340
// (verified: CalculateProposedLayout's `this` is that subobject, and it recovers
// the MCV with `lea rax,[this-0x340]` before calling the MCV-relative helpers).
inline constexpr uintptr_t MCV_layout_base      = 0x340; // LayoutManagerBase subobject within MCV
inline constexpr uintptr_t MCV_containers_data = 0x368;  // ContentsContainerView** (vector begin)
inline constexpr uintptr_t MCV_containers_count = 0x370; // size_t
inline constexpr uintptr_t MCV_resize_area      = 0x3f0; // EdgeMultiContentsResizeArea* (single)
inline constexpr uintptr_t MCV_orientation      = 0x428; // SplitTabLayout enum (0 = one axis, != 0 the other)
inline constexpr uintptr_t MCV_split_ratio      = 0x430; // double (default 0.5)
inline constexpr uintptr_t MCV_drop_target_view = 0x0b0; // views::View* (relative to the View base)
// ContentsContainerView
inline constexpr uintptr_t CCV_contents_web_view = 0x3a8; // ContentsWebView*
inline constexpr uintptr_t CCV_is_active         = 0x38d; // bool

// views::ProposedLayout (the out-param CalculateProposedLayout fills; ctor zeroes 0x20)
inline constexpr uintptr_t PL_host_size     = 0x00;  // gfx::Size {int w, int h}
inline constexpr uintptr_t PL_child_layouts = 0x08;  // std::vector<ChildLayout> {begin, end, cap}

// views::ChildLayout (element of PL_child_layouts; stride 0x30, from ApplyLayout)
inline constexpr uintptr_t CL_stride   = 0x30;
inline constexpr uintptr_t CL_child    = 0x00;  // views::View*
inline constexpr uintptr_t CL_visible  = 0x08;  // bool
inline constexpr uintptr_t CL_bounds   = 0x0c;  // gfx::Rect {int x, y, w, h}
inline constexpr uintptr_t CL_avail    = 0x1c;  // views::SizeBounds {SizeBound w{val,has}, h{val,has}}

// gfx::Rect field offsets within CL_bounds
inline constexpr uintptr_t RECT_x = 0x0, RECT_y = 0x4, RECT_w = 0x8, RECT_h = 0xc;
}  // namespace off

}  // namespace fixedge::edge155

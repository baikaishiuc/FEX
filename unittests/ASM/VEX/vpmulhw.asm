%ifdef CONFIG
{
  "HostFeatures": ["AVX"],
  "RegData": {
    "XMM2":  ["0x18D21A1D1B70CF3C", "0x24092594272728C2", "0x0000000000000000", "0x0000000000000000"],
    "XMM3":  ["0x18D21A1D1B70CF3C", "0x24092594272728C2", "0x0000000000000000", "0x0000000000000000"],
    "XMM4":  ["0x18D21A1D1B70CF3C", "0x24092594272728C2", "0x18D21A1D1B70CF3C", "0x24092594272728C2"],
    "XMM5":  ["0x18D21A1D1B70CF3C", "0x24092594272728C2", "0x18D21A1D1B70CF3C", "0x24092594272728C2"]
  },
  "MemoryRegions": {
    "0x100000000": "4096"
  }
}
%endif

lea rdx, [rel .data]

vmovapd ymm0, [rdx]
vmovapd ymm1, [rdx + 32]

vpmulhw xmm2, xmm0, xmm1
vpmulhw xmm3, xmm0, [rdx + 32]

vpmulhw ymm4, ymm0, ymm1
vpmulhw ymm5, ymm0, [rdx + 32]

hlt

align 32
.data:
dq 0x4142434445468748
dq 0x5152535455565758
dq 0x4142434445468748
dq 0x5152535455565758

dq 0x6162636465666768
dq 0x7172737475767778
dq 0x6162636465666768
dq 0x7172737475767778

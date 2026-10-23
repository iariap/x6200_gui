# Render benchmark — ROM194-AA

- Status: **INCONCLUSIVE** (exit 3)
- Date: 2026-10-21
- a: `c888308eede71205605c06383432ee7e165f202e`
- b: `c888308eede71205605c06383432ee7e165f202e`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| app-bands | PASS | 3.6601 | 4.0487 | 4.1468 | 273.21 | -17.08% | 0.0% | 0.0% |
| app-close-home | PASS | 3.7226 | 3.8624 | 3.8794 | 268.63 | -25.061% | 0.0% | 0.0% |
| apps-menu-1 | PASS | 3.7302 | 4.1372 | 4.2406 | 268.09 | 1.458% | 0.0% | 0.0% |
| apps-menu-2 | PASS | 3.7462 | 4.0143 | 4.3272 | 266.94 | -17.265% | 0.0% | 0.0% |
| apps-menu-3 | PASS | 3.8268 | 4.5108 | 4.6004 | 261.31 | -8.939% | 0.0% | 0.0% |
| apps-menu-4 | PASS | 3.7209 | 3.9393 | 3.9535 | 268.75 | -4.908% | 0.0% | 0.0% |
| apps-menu-5 | PASS | 3.7442 | 3.9253 | 3.9262 | 267.08 | -8.027% | 0.0% | 0.0% |
| apps-menu-6 | PASS | 3.8448 | 4.7039 | 4.7779 | 260.09 | 12.231% | 0.0% | 0.0% |
| autotune | PASS | 3.7504 | 4.1559 | 4.1959 | 266.63 | -11.04% | 0.0% | 0.0% |
| autotune-found | PASS | 3.6471 | 4.3471 | 4.5272 | 274.19 | 3.409% | 0.0% | 0.0% |
| autotune-notcw | PASS | 3.6811 | 3.8972 | 4.1523 | 271.66 | -17.085% | 0.0% | 0.0% |
| autotune-progress | PASS | 3.6765 | 3.9712 | 4.007 | 271.99 | -2.849% | 0.0% | 0.0% |
| bench-render | PASS | 4.0819 | 4.5079 | 4.7657 | 244.98 | -13.833% | 0.0% | 0.0% |
| bt-dialog | PASS | 6.9375 | 7.2305 | 7.2942 | 144.14 | -10.566% | 0.0% | 0.0% |
| bt-dialog-tema-aurora | PASS | 7.0523 | 8.3662 | 8.5471 | 141.8 | 10.351% | 0.0% | 0.0% |
| bt-dialog-tema-cathode | PASS | 7.3524 | 8.2666 | 8.2728 | 136.01 | 4.701% | 0.0% | 0.0% |
| bt-dialog-tema-redshift | PASS | 7.1228 | 7.4807 | 7.7121 | 140.39 | -15.78% | 0.0% | 0.0% |
| bt-dialog-tema-sprint | PASS | 7.1427 | 7.543 | 7.6788 | 140.0 | -20.718% | 0.0% | 0.0% |
| bt-dialog-tema-sunlit | PASS | 6.813 | 6.9885 | 7.0078 | 146.78 | -6.246% | 0.0% | 0.0% |
| bt-dialog-tema-trail | PASS | 7.0872 | 8.2207 | 8.2761 | 141.1 | 3.045% | 0.0% | 0.0% |
| cw-autotune-off | PASS | 3.5682 | 3.7476 | 3.7756 | 280.26 | -15.481% | 0.0% | 0.0% |
| cw-autotune-on | PASS | 3.5718 | 4.0107 | 4.141 | 279.97 | -15.107% | 0.0% | 0.0% |
| cw-tune-default | PASS | 3.6677 | 3.9411 | 3.9525 | 272.65 | -9.544% | 0.0% | 0.0% |
| cw-tune-ssb | PASS | 3.5796 | 3.8755 | 3.9047 | 279.36 | -11.717% | 0.0% | 0.0% |
| cw-tune-tema-sunlit | PASS | 3.5276 | 4.0525 | 4.3482 | 283.48 | -17.738% | 0.0% | 0.0% |
| cw-tune-tuner | PASS | 3.6811 | 4.0158 | 4.0199 | 271.66 | -5.867% | 0.0% | 0.0% |
| dfl-editor-cycles | PASS | 3.6795 | 4.2397 | 4.717 | 271.78 | 3.742% | 0.0% | 0.0% |
| dfl-editor-interaction | PASS | 4.2506 | 4.599 | 4.8262 | 235.26 | -4.611% | 0.0% | 0.0% |
| dfl-editor-lsb-d | PASS | 4.2238 | 4.6077 | 4.6843 | 236.76 | -12.756% | 0.0% | 0.0% |
| dfl-editor-reset | PASS | 4.2283 | 5.4295 | 6.2153 | 236.5 | -2.917% | 0.0% | 0.0% |
| dfl-editor-rotary0 | PASS | 3.7218 | 4.4739 | 4.9944 | 268.69 | 7.313% | 0.0% | 0.0% |
| dfl-editor-sel-bw-tema-aurora | PASS | 4.3881 | 5.1064 | 5.3997 | 227.89 | 6.594% | 0.0% | 0.0% |
| dfl-editor-sel-bw-tema-cathode | PASS | 4.3368 | 4.6304 | 4.9786 | 230.59 | -2.289% | 0.0% | 0.0% |
| dfl-editor-sel-bw-tema-redshift | PASS | 4.1358 | 4.9239 | 5.3929 | 241.79 | -1.849% | 0.0% | 0.0% |
| dfl-editor-sel-bw-tema-sprint | PASS | 4.1815 | 5.1626 | 5.278 | 239.15 | -23.097% | 0.0% | 0.0% |
| dfl-editor-sel-bw-tema-sunlit | PASS | 4.1174 | 4.678 | 4.8007 | 242.87 | 0.878% | 0.0% | 0.0% |
| dfl-editor-sel-bw-tema-trail | PASS | 4.3068 | 4.6189 | 4.8935 | 232.19 | -8.735% | 0.0% | 0.0% |
| dfl-editor-sel-high-tema-aurora | PASS | 4.2309 | 5.9394 | 6.0404 | 236.36 | 22.515% | 0.0% | 0.0% |
| dfl-editor-sel-high-tema-cathode | PASS | 4.254 | 4.4176 | 4.466 | 235.07 | -21.681% | 0.0% | 0.0% |
| dfl-editor-sel-high-tema-redshift | PASS | 4.2639 | 4.9906 | 5.0804 | 234.52 | 14.419% | 0.0% | 0.0% |
| dfl-editor-sel-high-tema-sprint | PASS | 4.4056 | 4.8365 | 5.0668 | 226.98 | 7.576% | 0.0% | 0.0% |
| dfl-editor-sel-high-tema-sunlit | PASS | 4.1463 | 4.5273 | 4.5531 | 241.18 | -21.913% | 0.0% | 0.0% |
| dfl-editor-sel-high-tema-trail | PASS | 4.2888 | 4.863 | 4.93 | 233.17 | -1.645% | 0.0% | 0.0% |
| dfl-editor-tx | PASS | 4.3848 | 4.6132 | 4.6958 | 228.06 | -2.209% | 0.0% | 0.0% |
| dfl-editor-usb-d | PASS | 4.6337 | 4.9537 | 5.0515 | 215.81 | 4.848% | 0.0% | 0.0% |
| dfl-editor-usb-d-tema-aurora | PASS | 4.3869 | 4.8595 | 4.8745 | 227.95 | 0.909% | 0.0% | 0.0% |
| dfl-editor-usb-d-tema-cathode | PASS | 4.2751 | 5.4086 | 6.0186 | 233.92 | -0.425% | 0.0% | 0.0% |
| dfl-editor-usb-d-tema-redshift | PASS | 4.231 | 4.4364 | 4.6769 | 236.35 | -18.92% | 0.0% | 0.0% |
| dfl-editor-usb-d-tema-sprint | PASS | 4.0886 | 4.7321 | 4.9351 | 244.58 | -2.969% | 0.0% | 0.0% |
| dfl-editor-usb-d-tema-sunlit | PASS | 4.1958 | 5.968 | 6.0934 | 238.33 | 15.386% | 0.0% | 0.0% |
| dfl-editor-usb-d-tema-trail | PASS | 4.1924 | 4.3736 | 4.4089 | 238.53 | -0.086% | 0.0% | 0.0% |
| dfl-lsb-d | PASS | 3.8956 | 4.0356 | 4.0422 | 256.7 | -5.721% | 0.0% | 0.0% |
| dfl-mirror-lsb | PASS | 4.5132 | 5.0901 | 5.2494 | 221.57 | 12.537% | 0.0% | 0.0% |
| dfl-mirror-usb | PASS | 4.0885 | 4.5088 | 4.6251 | 244.59 | -2.333% | 0.0% | 0.0% |
| dfl-page-tema-aurora | PASS | 4.0215 | 4.3348 | 4.3563 | 248.66 | -21.95% | 0.0% | 0.0% |
| dfl-page-tema-cathode | PASS | 3.9917 | 4.5541 | 5.026 | 250.52 | 1.741% | 0.0% | 0.0% |
| dfl-page-tema-redshift | PASS | 3.9568 | 4.6643 | 4.7341 | 252.73 | 17.332% | 0.0% | 0.0% |
| dfl-page-tema-sprint | PASS | 4.0209 | 4.5598 | 5.0891 | 248.7 | -14.041% | 0.0% | 0.0% |
| dfl-page-tema-sunlit | PASS | 4.0383 | 4.277 | 4.2836 | 247.63 | 3.675% | 0.0% | 0.0% |
| dfl-page-tema-trail | PASS | 3.9038 | 4.4274 | 4.8512 | 256.16 | 6.099% | 0.0% | 0.0% |
| dfl-presets-activo | PASS | 4.2127 | 4.4136 | 4.577 | 237.38 | -2.131% | 0.0% | 0.0% |
| dfl-presets-adj-tema-aurora | PASS | 3.8777 | 4.323 | 4.3775 | 257.89 | -21.801% | 0.0% | 0.0% |
| dfl-presets-adj-tema-cathode | PASS | 3.9216 | 4.2575 | 4.7249 | 255.0 | 1.928% | 0.0% | 0.0% |
| dfl-presets-adj-tema-redshift | PASS | 3.8841 | 4.6102 | 4.8793 | 257.46 | 1.126% | 0.0% | 0.0% |
| dfl-presets-adj-tema-sprint | PASS | 3.9808 | 4.1707 | 4.202 | 251.21 | -8.602% | 0.0% | 0.0% |
| dfl-presets-adj-tema-sunlit | PASS | 3.8438 | 4.6627 | 4.8413 | 260.16 | -0.906% | 0.0% | 0.0% |
| dfl-presets-adj-tema-trail | PASS | 4.055 | 4.7946 | 5.0166 | 246.61 | 17.055% | 0.0% | 0.0% |
| dfl-presets-apply | PASS | 4.0953 | 4.3445 | 4.8198 | 244.19 | -6.947% | 0.0% | 0.0% |
| dfl-presets-assign-cancel | PASS | 4.0161 | 4.6766 | 5.218 | 248.99 | 4.058% | 0.0% | 0.0% |
| dfl-presets-assign-ok | PASS | 3.8151 | 3.9737 | 3.9859 | 262.12 | -20.38% | 0.0% | 0.0% |
| dfl-presets-assign-tema-aurora | PASS | 4.0092 | 4.2216 | 4.2588 | 249.43 | -7.446% | 0.0% | 0.0% |
| dfl-presets-assign-tema-cathode | PASS | 4.0794 | 4.3396 | 4.5738 | 245.13 | -2.905% | 0.0% | 0.0% |
| dfl-presets-assign-tema-redshift | PASS | 4.2105 | 4.419 | 4.4975 | 237.5 | -5.464% | 0.0% | 0.0% |
| dfl-presets-assign-tema-sprint | PASS | 4.1502 | 5.0191 | 5.1005 | 240.95 | 5.102% | 0.0% | 0.0% |
| dfl-presets-assign-tema-sunlit | PASS | 4.0705 | 4.685 | 4.8909 | 245.67 | 4.844% | 0.0% | 0.0% |
| dfl-presets-assign-tema-trail | PASS | 4.0925 | 4.5053 | 4.5343 | 244.35 | 0.607% | 0.0% | 0.0% |
| dfl-presets-confirm-tema-aurora | PASS | 4.1776 | 4.9613 | 5.0993 | 239.37 | 11.683% | 0.0% | 0.0% |
| dfl-presets-confirm-tema-cathode | PASS | 4.2716 | 4.8788 | 5.2202 | 234.1 | 10.01% | 0.0% | 0.0% |
| dfl-presets-confirm-tema-redshift | INCONCLUSIVE | 4.1489 | 6.2181 | 6.5655 | 241.03 | 30.612% | 0.0% | 0.0% |
| dfl-presets-confirm-tema-sprint | PASS | 4.0456 | 5.4134 | 6.7958 | 247.18 | 13.221% | 0.0% | 0.0% |
| dfl-presets-confirm-tema-sunlit | PASS | 4.0705 | 4.5568 | 4.9242 | 245.67 | 5.415% | 0.0% | 0.0% |
| dfl-presets-confirm-tema-trail | PASS | 4.0496 | 4.4592 | 4.6944 | 246.93 | -13.239% | 0.0% | 0.0% |
| dfl-presets-custom | PASS | 4.0528 | 5.0927 | 5.1153 | 246.74 | 14.734% | 0.0% | 0.0% |
| dfl-presets-cycles | PASS | 3.3518 | 3.5338 | 3.5695 | 298.34 | -6.397% | 0.0% | 0.0% |
| dfl-presets-digi | PASS | 4.276 | 5.064 | 5.35 | 233.86 | 17.263% | 0.0% | 0.0% |
| dfl-presets-fav-tema-aurora | PASS | 3.9775 | 4.3046 | 4.3066 | 251.41 | 2.699% | 0.0% | 0.0% |
| dfl-presets-fav-tema-cathode | PASS | 4.0928 | 4.3968 | 4.8355 | 244.33 | 2.915% | 0.0% | 0.0% |
| dfl-presets-fav-tema-redshift | PASS | 3.9885 | 4.2555 | 4.2851 | 250.72 | 7.619% | 0.0% | 0.0% |
| dfl-presets-fav-tema-sprint | PASS | 4.133 | 4.6192 | 5.091 | 241.95 | -4.047% | 0.0% | 0.0% |
| dfl-presets-fav-tema-sunlit | INCONCLUSIVE | 4.4902 | 5.8131 | 5.8555 | 222.71 | 27.341% | 0.0% | 0.0% |
| dfl-presets-fav-tema-trail | PASS | 4.0209 | 4.2947 | 4.3705 | 248.7 | -6.225% | 0.0% | 0.0% |
| dfl-presets-graph-tema-aurora | PASS | 4.2968 | 5.358 | 5.4581 | 232.73 | 21.792% | 0.0% | 0.0% |
| dfl-presets-graph-tema-cathode | PASS | 4.3997 | 4.7574 | 4.8051 | 227.29 | 6.491% | 0.0% | 0.0% |
| dfl-presets-graph-tema-redshift | PASS | 4.2438 | 4.8324 | 5.0867 | 235.64 | 12.346% | 0.0% | 0.0% |
| dfl-presets-graph-tema-sprint | PASS | 4.4028 | 4.9673 | 5.1895 | 227.13 | 5.772% | 0.0% | 0.0% |
| dfl-presets-graph-tema-sunlit | PASS | 4.1835 | 4.789 | 5.0698 | 239.03 | 2.96% | 0.0% | 0.0% |
| dfl-presets-graph-tema-trail | PASS | 4.4588 | 5.2093 | 5.6706 | 224.28 | 19.741% | 0.0% | 0.0% |
| dfl-presets-mode-change | PASS | 3.9228 | 4.2269 | 4.3167 | 254.92 | -11.848% | 0.0% | 0.0% |
| dfl-presets-more-tema-aurora | PASS | 4.1612 | 5.3554 | 5.4233 | 240.31 | 9.164% | 0.0% | 0.0% |
| dfl-presets-more-tema-cathode | PASS | 4.2097 | 4.5594 | 4.7191 | 237.55 | 4.922% | 0.0% | 0.0% |
| dfl-presets-more-tema-redshift | INCONCLUSIVE | 3.8912 | 6.6828 | 6.705 | 256.99 | 43.474% | 0.0% | 0.0% |
| dfl-presets-more-tema-sprint | PASS | 3.9865 | 5.0805 | 5.1589 | 250.85 | 13.348% | 0.0% | 0.0% |
| dfl-presets-more-tema-sunlit | PASS | 3.9819 | 4.6281 | 4.6931 | 251.14 | 6.53% | 0.0% | 0.0% |
| dfl-presets-more-tema-trail | PASS | 4.0563 | 4.9913 | 5.0115 | 246.53 | 24.898% | 0.0% | 0.0% |
| dfl-presets-nav | PASS | 3.9066 | 4.6209 | 4.8154 | 255.98 | 22.477% | 0.0% | 0.0% |
| dfl-presets-nfm | PASS | 3.6822 | 3.9339 | 3.9487 | 271.58 | -3.093% | 0.0% | 0.0% |
| dfl-presets-rotary0 | PASS | 3.8973 | 4.5789 | 4.6583 | 256.59 | -25.251% | 0.0% | 0.0% |
| dfl-presets-save | PASS | 3.9024 | 5.4067 | 5.4458 | 256.25 | 4.769% | 0.0% | 0.0% |
| dfl-presets-save-cancel | PASS | 4.054 | 4.3576 | 4.5267 | 246.67 | -23.139% | 0.0% | 0.0% |
| dfl-presets-save-fail | PASS | 4.0811 | 4.8842 | 5.2551 | 245.03 | 5.436% | 0.0% | 0.0% |
| dfl-presets-touch | PASS | 4.023 | 4.4013 | 4.5708 | 248.57 | -22.583% | 0.0% | 0.0% |
| dfl-presets-touch-hold | PASS | 4.1506 | 4.6666 | 4.9606 | 240.93 | 0.414% | 0.0% | 0.0% |
| dfl-presets-tx | PASS | 4.0581 | 5.5824 | 5.8981 | 246.42 | 13.555% | 0.0% | 0.0% |
| dfl-presets-vol | PASS | 3.967 | 4.2705 | 4.3154 | 252.08 | -13.561% | 0.0% | 0.0% |
| dfl-usb-d | PASS | 4.0776 | 4.8209 | 4.846 | 245.25 | -18.436% | 0.0% | 0.0% |
| dfl-usb-d-span100k | PASS | 3.8669 | 4.338 | 4.352 | 258.61 | -9.951% | 0.0% | 0.0% |
| dfl-usb-d-span50k | PASS | 4.0011 | 4.3497 | 4.6661 | 249.93 | -7.81% | 0.0% | 0.0% |
| dfl-usb-d-tema-cathode | PASS | 4.0537 | 4.9434 | 5.004 | 246.69 | -6.173% | 0.0% | 0.0% |
| dfl-usb-d-tema-redshift | PASS | 3.9458 | 4.1115 | 4.1286 | 253.44 | -11.404% | 0.0% | 0.0% |
| dfl-usb-d-tema-sprint | PASS | 4.1113 | 4.6667 | 4.7125 | 243.23 | -9.93% | 0.0% | 0.0% |
| dfl-usb-d-tema-sunlit | PASS | 3.9189 | 4.025 | 4.0567 | 255.17 | -7.316% | 0.0% | 0.0% |
| dfl-usb-d-tema-trail | PASS | 4.0028 | 4.2903 | 4.4676 | 249.83 | -6.081% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.8616 | 4.062 | 4.1498 | 258.96 | -0.14% | 0.0% | 0.0% |
| home-cb | PASS | 4.1087 | 4.6683 | 4.7839 | 243.39 | -13.215% | 0.0% | 0.0% |
| home-cb-a | PASS | 3.7214 | 4.1564 | 4.7974 | 268.72 | 1.033% | 0.0% | 0.0% |
| home-cb-cathode | PASS | 3.8858 | 4.1739 | 4.1804 | 257.35 | -3.515% | 0.0% | 0.0% |
| home-cb-d | PASS | 3.7891 | 4.2373 | 4.3003 | 263.92 | 11.034% | 0.0% | 0.0% |
| home-cb-offchannel | PASS | 3.753 | 4.2635 | 4.344 | 266.45 | 0.025% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.8373 | 4.4861 | 4.5143 | 260.6 | -23.429% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.9047 | 4.3207 | 4.3646 | 256.1 | -11.215% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.8597 | 5.0506 | 5.5772 | 259.09 | 23.97% | 0.0% | 0.0% |
| main-espectro-agua | INCONCLUSIVE | 3.9914 | 5.7699 | 6.3543 | 250.54 | 28.525% | 0.0% | 0.0% |
| main-foco | INCONCLUSIVE | 3.6025 | 6.4958 | 6.688 | 277.58 | 54.387% | 0.0% | 0.0% |
| main-frecuencia-dseg | PASS | 3.8052 | 4.3132 | 4.3509 | 262.8 | -4.322% | 0.0% | 0.0% |
| main-natural | PASS | 3.7606 | 5.078 | 5.2262 | 265.92 | 16.186% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.7217 | 4.5417 | 4.87 | 268.69 | 4.477% | 0.0% | 0.0% |
| main-tema-aurora | PASS | 3.9362 | 4.8747 | 5.1281 | 254.05 | 4.503% | 0.0% | 0.0% |
| main-tema-aurora-agua | PASS | 3.9053 | 4.664 | 4.9179 | 256.07 | 7.029% | 0.0% | 0.0% |
| main-tema-aurora-eco | PASS | 3.7062 | 4.2674 | 4.5779 | 269.82 | -13.709% | 0.0% | 0.0% |
| main-tema-aurora-tx | PASS | 3.5674 | 4.2234 | 4.2585 | 280.32 | -6.037% | 0.0% | 0.0% |
| main-tema-cathode | PASS | 3.8787 | 4.1116 | 4.2358 | 257.82 | -7.844% | 0.0% | 0.0% |
| main-tema-cathode-agua | PASS | 3.8899 | 4.036 | 4.0446 | 257.07 | -5.646% | 0.0% | 0.0% |
| main-tema-cathode-eco | PASS | 3.719 | 4.0061 | 4.1482 | 268.89 | 1.485% | 0.0% | 0.0% |
| main-tema-cathode-tx | PASS | 3.699 | 4.3296 | 4.5099 | 270.34 | 4.932% | 0.0% | 0.0% |
| main-tema-redshift | PASS | 3.6998 | 4.1212 | 4.4037 | 270.28 | -6.605% | 0.0% | 0.0% |
| main-tema-redshift-agua | PASS | 3.8995 | 4.3145 | 4.4868 | 256.44 | -22.78% | 0.0% | 0.0% |
| main-tema-redshift-eco | PASS | 3.6817 | 4.5052 | 4.5347 | 271.61 | -18.159% | 0.0% | 0.0% |
| main-tema-redshift-tx | PASS | 3.7674 | 4.1093 | 5.0978 | 265.44 | -1.104% | 0.0% | 0.0% |
| main-tema-sprint | PASS | 3.597 | 4.484 | 4.7019 | 278.01 | -10.018% | 0.0% | 0.0% |
| main-tema-sprint-agua | PASS | 3.7462 | 4.3383 | 4.3741 | 266.94 | -10.728% | 0.0% | 0.0% |
| main-tema-sprint-eco | PASS | 3.5629 | 4.0131 | 4.026 | 280.67 | -33.017% | 0.0% | 0.0% |
| main-tema-sprint-tx | PASS | 3.9707 | 4.7199 | 4.7312 | 251.84 | -5.838% | 0.0% | 0.0% |
| main-tema-sunlit | PASS | 3.5591 | 3.9366 | 4.1035 | 280.97 | -21.793% | 0.0% | 0.0% |
| main-tema-sunlit-agua | PASS | 3.9112 | 4.1371 | 4.2292 | 255.68 | -1.357% | 0.0% | 0.0% |
| main-tema-sunlit-eco | PASS | 3.7607 | 4.4871 | 4.7283 | 265.91 | -6.585% | 0.0% | 0.0% |
| main-tema-sunlit-tx | PASS | 3.7023 | 4.2551 | 4.3713 | 270.1 | 4.135% | 0.0% | 0.0% |
| main-tema-trail | PASS | 3.8342 | 4.3541 | 4.4126 | 260.81 | 0.319% | 0.0% | 0.0% |
| main-tema-trail-agua | PASS | 3.9563 | 4.6896 | 4.701 | 252.76 | -3.391% | 0.0% | 0.0% |
| main-tema-trail-eco | PASS | 3.7811 | 4.5101 | 4.5819 | 264.47 | -4.599% | 0.0% | 0.0% |
| main-tema-trail-tx | PASS | 3.853 | 4.5427 | 4.6371 | 259.54 | 9.839% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.8653 | 4.6822 | 4.807 | 258.71 | 13.615% | 0.0% | 0.0% |
| settings-audio | PASS | 7.3792 | 9.1186 | 9.1312 | 135.52 | 8.153% | 0.0% | 0.0% |
| settings-bands | PASS | 3.7196 | 4.0312 | 4.6207 | 268.84 | 7.732% | 0.0% | 0.0% |
| settings-grupo-actions | PASS | 7.0177 | 7.8255 | 8.1197 | 142.5 | 0.002% | 0.0% | 0.0% |
| settings-grupo-display | PASS | 7.1592 | 7.3886 | 7.595 | 139.68 | -16.704% | 0.0% | 0.0% |
| settings-grupo-info | PASS | 7.4542 | 8.3961 | 8.5869 | 134.15 | -19.644% | 0.0% | 0.0% |
| settings-grupo-operation | PASS | 7.1662 | 7.9509 | 8.336 | 139.54 | 3.39% | 0.0% | 0.0% |
| settings-grupo-operation-tema-aurora | PASS | 7.0897 | 8.2974 | 8.3144 | 141.05 | 8.706% | 0.0% | 0.0% |
| settings-grupo-operation-tema-cathode | PASS | 7.0922 | 7.3934 | 7.6302 | 141.0 | -9.936% | 0.0% | 0.0% |
| settings-grupo-operation-tema-redshift | PASS | 7.0669 | 8.5317 | 9.7659 | 141.5 | -1.159% | 0.0% | 0.0% |
| settings-grupo-operation-tema-sprint | PASS | 7.0336 | 7.9064 | 8.2997 | 142.17 | -26.295% | 0.0% | 0.0% |
| settings-grupo-operation-tema-sunlit | PASS | 7.1555 | 7.7267 | 7.8205 | 139.75 | -6.45% | 0.0% | 0.0% |
| settings-grupo-operation-tema-trail | PASS | 7.195 | 7.6522 | 7.8776 | 138.99 | -10.822% | 0.0% | 0.0% |
| settings-home | PASS | 7.2936 | 7.7954 | 7.9393 | 137.11 | -6.606% | 0.0% | 0.0% |
| settings-perfil | PASS | 7.2248 | 9.0959 | 9.3739 | 138.41 | 7.17% | 0.0% | 0.0% |
| settings-tema-aurora | PASS | 7.0217 | 8.1847 | 8.4445 | 142.41 | -0.315% | 0.0% | 0.0% |
| settings-tema-cathode | PASS | 7.3729 | 9.4366 | 9.5204 | 135.63 | 3.28% | 0.0% | 0.0% |
| settings-tema-redshift | PASS | 7.1648 | 7.9495 | 8.3121 | 139.57 | 1.105% | 0.0% | 0.0% |
| settings-tema-sprint | PASS | 7.1893 | 7.6517 | 7.9827 | 139.1 | 2.194% | 0.0% | 0.0% |
| settings-tema-sunlit | PASS | 7.1112 | 8.2688 | 8.3482 | 140.62 | -4.612% | 0.0% | 0.0% |
| settings-tema-trail | PASS | 7.1876 | 7.5562 | 7.8915 | 139.13 | -5.527% | 0.0% | 0.0% |
| settings-wfbat | PASS | 7.7119 | 8.1136 | 8.1186 | 129.67 | -1.804% | 0.0% | 0.0% |
| swrscan-3pass | PASS | 7.477 | 8.0757 | 8.4356 | 133.74 | 7.608% | 0.0% | 0.0% |
| swrscan-3pass-p1 | PASS | 7.5619 | 8.6502 | 8.8024 | 132.24 | 8.325% | 0.0% | 0.0% |
| swrscan-3pass-p2 | PASS | 7.1172 | 7.3968 | 7.4499 | 140.5 | -9.487% | 0.0% | 0.0% |
| swrscan-3pass-p3 | PASS | 7.2455 | 8.0045 | 8.7059 | 138.02 | -5.971% | 0.0% | 0.0% |
| swrscan-3pass-stop | PASS | 7.0935 | 8.4779 | 8.5941 | 140.98 | 11.44% | 0.0% | 0.0% |
| swrscan-3pass-tema-aurora | PASS | 7.3544 | 7.6384 | 7.656 | 135.97 | -3.471% | 0.0% | 0.0% |
| swrscan-3pass-tema-cathode | PASS | 7.3958 | 8.1791 | 8.3199 | 135.21 | -5.87% | 0.0% | 0.0% |
| swrscan-5pass-moving | PASS | 7.2825 | 8.8595 | 8.9505 | 137.32 | 15.245% | 0.0% | 0.0% |
| swrscan-5pass-outliers | PASS | 7.2162 | 8.3906 | 8.5405 | 138.58 | 12.298% | 0.0% | 0.0% |
| swrscan-double-valley | PASS | 7.271 | 7.9688 | 8.3894 | 137.53 | -9.294% | 0.0% | 0.0% |
| swrscan-flat | PASS | 6.9738 | 7.7661 | 7.8782 | 143.39 | -0.799% | 0.0% | 0.0% |
| swrscan-narrow-zoom | PASS | 6.9878 | 7.6821 | 7.7463 | 143.11 | 3.118% | 0.0% | 0.0% |
| swrscan-owner-40m | PASS | 7.0749 | 8.662 | 8.7617 | 141.34 | 4.646% | 0.0% | 0.0% |
| swrscan-owner-40m-domo | PASS | 7.8367 | 9.235 | 9.6836 | 127.61 | 1.202% | 0.0% | 0.0% |
| swrscan-owner-40m-sweep | PASS | 7.0977 | 7.8622 | 7.9372 | 140.89 | -4.211% | 0.0% | 0.0% |
| swrscan-quick | PASS | 7.5419 | 8.029 | 8.1676 | 132.59 | 8.285% | 0.0% | 0.0% |
| swrscan-wide-zoom | PASS | 6.9982 | 7.2373 | 7.4725 | 142.89 | -13.314% | 0.0% | 0.0% |

- **dfl-presets-confirm-tema-redshift** (INCONCLUSIVE):
  - p95 render +30.6% (budget 25.0%): 4.761 -> 6.218 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **dfl-presets-fav-tema-sunlit** (INCONCLUSIVE):
  - p95 render +27.3% (budget 25.0%): 4.565 -> 5.813 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **dfl-presets-more-tema-redshift** (INCONCLUSIVE):
  - p95 render +43.5% (budget 25.0%): 4.658 -> 6.683 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-espectro-agua** (INCONCLUSIVE):
  - p95 render +28.5% (budget 25.0%): 4.489 -> 5.770 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-foco** (INCONCLUSIVE):
  - p95 render +54.4% (budget 25.0%): 4.208 -> 6.496 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- Environment: x86 host: relative regression proxy, not the Cortex-A7

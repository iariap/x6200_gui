# X6200 `.xgf` — descifrado AES-256-ECB verificado

## Clave y procedimiento

Clave pública de **G90Tools** usada en el handoff (64 hex chars); no se publica aquí como secreto
porque ya es pública. El procedimiento es AES-256-ECB sin padding, 16 B por bloque.

```bash
openssl enc -d -aes-256-ecb -nopad -K "$KEY" -in firmware.xgf -out firmware-dec.bin
openssl enc -e -aes-256-ecb -nopad -K "$KEY" -in firmware-dec.bin -out roundtrip.xgf
cmp firmware.xgf roundtrip.xgf
```

## Validación

| Archivo | Entrada | Descifrado |
|---|---|---|
| 1.0.6 | SHA256 `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3` | 524288 B, SHA256 `1b7606b657a89d011682ea6f41a84a910d76ab5fe5526f82f5dc54d86cc18685` |
| 1.0.8 | SHA256 `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23` | 524288 B, SHA256 `c3f7d028bc63e43fbb478a09e1032d46059b71efe217a60b576231866fbc2c0a` |

- **Round-trip exacto** para ambos: reencriptar el descifrado reproduce byte a byte el `.xgf`.
- Bloques conocidos validados: padding `FF×16`, `00×16`, y punteros little-endian
  `0x08002715`, `0x08024A11`, `0x080248D1`, `0x0802F0D9`.
- **La imagen descifrada tiene sentido de firmware ARM Cortex-M7**: vector table con SP
  `0x24050000`, reset `0x08024881`, handlers Thumb, strings/código.
- `XGF_DECRYPTION = VERIFIED`.

No se publica el binario completo; hashes y diffs en la evidencia.

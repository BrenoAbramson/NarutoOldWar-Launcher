# Naruto Old War Launcher

Launcher e atualizador do cliente Naruto Old War.

## Ideia

1. O jogador abre o launcher.
2. O launcher consulta `updates/latest.json`.
3. Se houver versao nova, baixa o ZIP do Windows ou macOS.
4. Extrai o ZIP por cima da pasta do cliente.
5. Abre o `OTClient.exe` no Windows ou `OTClient.app` no macOS.

## Estrutura de update

Publique um arquivo parecido com `updates/latest.example.json`:

```json
{
  "version": "1.0.1",
  "changelog": ["Nova musica na tela de login"],
  "windows": {
    "url": "https://github.com/BrenoAbramson/Servidor/releases/download/client-1.0.1/OTClient-windows.zip"
  },
  "macos": {
    "url": "https://github.com/BrenoAbramson/Servidor/releases/download/client-1.0.1/OTClient-macos.zip"
  }
}
```

## Desenvolvimento

Requisitos:

- Node.js
- Rust/Cargo
- Tauri CLI

Comandos:

```bash
npm install
npm run dev
npm run build
```

## Observacao

O primeiro MVP atualiza por ZIP completo. Depois ele pode evoluir para manifest
por arquivo com hash, baixando apenas o que mudou.

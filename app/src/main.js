const MANIFEST_URL = "https://raw.githubusercontent.com/BrenoAbramson/NarutoOldWar-Launcher/main/updates/latest.json";

const statusTitle = document.querySelector("#statusTitle");
const statusText = document.querySelector("#statusText");
const progressBar = document.querySelector("#progressBar");
const localVersion = document.querySelector("#localVersion");
const remoteVersion = document.querySelector("#remoteVersion");
const changelog = document.querySelector("#changelog");
const checkButton = document.querySelector("#checkButton");
const updateButton = document.querySelector("#updateButton");
const playButton = document.querySelector("#playButton");

let latestManifest = null;

function hasTauri() {
  return Boolean(window.__TAURI__?.core?.invoke);
}

async function invoke(command, args = {}) {
  if (!hasTauri()) {
    return mockInvoke(command);
  }

  return window.__TAURI__.core.invoke(command, args);
}

async function mockInvoke(command) {
  await new Promise((resolve) => setTimeout(resolve, 350));

  if (command === "get_local_version") {
    return "0.1.0-preview";
  }

  if (command === "check_updates") {
    return {
      version: "1.0.1",
      needsUpdate: true,
      changelog: [
        "Nova musica na tela de login",
        "Sistema de stages de experiencia",
        "Base inicial do launcher"
      ]
    };
  }

  if (command === "install_update") {
    for (const percent of [18, 38, 64, 82, 100]) {
      setProgress(percent);
      await new Promise((resolve) => setTimeout(resolve, 250));
    }
    return "Atualizacao instalada";
  }

  return "Modo preview";
}

function setStatus(title, text) {
  statusTitle.textContent = title;
  statusText.textContent = text;
}

function setProgress(value) {
  progressBar.style.width = `${Math.max(0, Math.min(100, value))}%`;
}

function renderChangelog(items = []) {
  changelog.innerHTML = "";
  const safeItems = items.length ? items : ["Sem changelog publicado para esta versao."];

  for (const item of safeItems) {
    const li = document.createElement("li");
    li.textContent = item;
    changelog.appendChild(li);
  }
}

async function loadLocalVersion() {
  const version = await invoke("get_local_version");
  localVersion.textContent = `Local: ${version}`;
}

async function checkUpdates() {
  setProgress(12);
  setStatus("Verificando atualizações", "Consultando o manifesto mais recente do cliente.");
  checkButton.disabled = true;

  try {
    latestManifest = await invoke("check_updates", { manifestUrl: MANIFEST_URL });
    remoteVersion.textContent = `Online: ${latestManifest.version}`;
    renderChangelog(latestManifest.changelog);
    setProgress(latestManifest.needsUpdate ? 45 : 100);

    if (latestManifest.needsUpdate) {
      setStatus("Atualização disponível", "Baixe a nova versão antes de entrar no jogo.");
      updateButton.disabled = false;
    } else {
      setStatus("Cliente atualizado", "Você já está com a versão mais recente.");
      updateButton.disabled = true;
    }
  } catch (error) {
    setProgress(0);
    setStatus("Falha ao verificar", String(error));
  } finally {
    checkButton.disabled = false;
  }
}

async function installUpdate() {
  if (!latestManifest) {
    await checkUpdates();
  }

  setProgress(8);
  setStatus("Baixando atualização", "Aguarde enquanto o launcher prepara o cliente.");
  updateButton.disabled = true;

  try {
    await invoke("install_update", { manifest: latestManifest });
    setProgress(100);
    await loadLocalVersion();
    setStatus("Atualização concluída", "Cliente pronto para jogar.");
  } catch (error) {
    setProgress(0);
    setStatus("Erro ao atualizar", String(error));
    updateButton.disabled = false;
  }
}

async function play() {
  setStatus("Abrindo cliente", "Iniciando Naruto Old War.");

  try {
    await invoke("launch_client");
  } catch (error) {
    setStatus("Não foi possível abrir o cliente", String(error));
  }
}

checkButton.addEventListener("click", checkUpdates);
updateButton.addEventListener("click", installUpdate);
playButton.addEventListener("click", play);

loadLocalVersion();

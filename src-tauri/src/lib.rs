use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::{
    env,
    fs::{self, File},
    io,
    path::{Path, PathBuf},
    process::Command,
};

const LOCAL_VERSION_FILE: &str = "launcher-version.json";

#[derive(Debug, Deserialize, Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct UpdateManifest {
    version: String,
    #[serde(default, alias = "notes")]
    changelog: Vec<String>,
    #[serde(default)]
    windows: PlatformPackage,
    #[serde(default)]
    macos: PlatformPackage,
    #[serde(default)]
    needs_update: bool,
}

#[derive(Debug, Default, Deserialize, Serialize, Clone)]
struct PlatformPackage {
    #[serde(default)]
    url: String,
}

#[derive(Debug, Deserialize, Serialize)]
struct LocalVersion {
    version: String,
}

fn install_dir() -> Result<PathBuf, String> {
    if let Ok(current_dir) = env::current_dir() {
        if current_dir.join(LOCAL_VERSION_FILE).exists() || current_dir.join(client_binary_name()).exists() {
            return Ok(current_dir);
        }

        if let Some(parent) = current_dir.parent() {
            if parent.join(LOCAL_VERSION_FILE).exists() || parent.join(client_binary_name()).exists() {
                return Ok(parent.to_path_buf());
            }
        }
    }

    let exe = env::current_exe().map_err(|error| error.to_string())?;
    let mut dir = exe
        .parent()
        .map(Path::to_path_buf)
        .ok_or_else(|| "Nao foi possivel localizar a pasta do launcher.".to_string())?;

    for _ in 0..6 {
        if dir.join(LOCAL_VERSION_FILE).exists() || dir.join(client_binary_name()).exists() {
            return Ok(dir);
        }

        if !dir.pop() {
            break;
        }
    }

    exe.parent()
        .map(Path::to_path_buf)
        .ok_or_else(|| "Nao foi possivel localizar a pasta do launcher.".to_string())
}

fn client_binary_name() -> &'static str {
    if cfg!(target_os = "macos") {
        "OTClient.app"
    } else {
        "OTClient.exe"
    }
}

fn version_file_path() -> Result<PathBuf, String> {
    Ok(install_dir()?.join(LOCAL_VERSION_FILE))
}

fn read_local_version() -> String {
    let Ok(path) = version_file_path() else {
        return "0.0.0".to_string();
    };

    let Ok(content) = fs::read_to_string(path) else {
        return "0.0.0".to_string();
    };

    serde_json::from_str::<LocalVersion>(&content)
        .map(|local| local.version)
        .unwrap_or_else(|_| "0.0.0".to_string())
}

fn write_local_version(version: &str) -> Result<(), String> {
    let content = serde_json::to_string_pretty(&LocalVersion {
        version: version.to_string(),
    })
    .map_err(|error| error.to_string())?;

    fs::write(version_file_path()?, content).map_err(|error| error.to_string())
}

fn platform_url(manifest: &UpdateManifest) -> String {
    if cfg!(target_os = "macos") {
        manifest.macos.url.clone()
    } else {
        manifest.windows.url.clone()
    }
}

fn normalize_manifest(mut value: Value) -> Result<UpdateManifest, String> {
    if let Some(platforms) = value.get("platforms").cloned() {
        if value.get("macos").is_none() {
            if let Some(macos) = platforms.get("macos") {
                value["macos"] = macos.clone();
            }
        }

        if value.get("windows").is_none() {
            if let Some(windows) = platforms.get("windows") {
                value["windows"] = windows.clone();
            }
        }
    }

    serde_json::from_value::<UpdateManifest>(value).map_err(|error| error.to_string())
}

fn extract_zip(zip_path: &Path, destination: &Path) -> Result<(), String> {
    let file = File::open(zip_path).map_err(|error| error.to_string())?;
    let mut archive = zip::ZipArchive::new(file).map_err(|error| error.to_string())?;

    for index in 0..archive.len() {
        let mut entry = archive.by_index(index).map_err(|error| error.to_string())?;
        let Some(enclosed_name) = entry.enclosed_name() else {
            continue;
        };
        let output_path = destination.join(enclosed_name);

        if entry.is_dir() {
            fs::create_dir_all(&output_path).map_err(|error| error.to_string())?;
            continue;
        }

        if let Some(parent) = output_path.parent() {
            fs::create_dir_all(parent).map_err(|error| error.to_string())?;
        }

        let mut output = File::create(&output_path).map_err(|error| error.to_string())?;
        io::copy(&mut entry, &mut output).map_err(|error| error.to_string())?;
    }

    Ok(())
}

#[tauri::command]
fn get_local_version() -> String {
    read_local_version()
}

#[tauri::command(rename_all = "camelCase")]
fn check_updates(manifest_url: String) -> Result<UpdateManifest, String> {
    let value = reqwest::blocking::Client::new()
        .get(manifest_url)
        .header("Cache-Control", "no-cache")
        .header("Pragma", "no-cache")
        .send()
        .map_err(|error| error.to_string())?
        .json::<Value>()
        .map_err(|error| error.to_string())?;
    let mut manifest = normalize_manifest(value)?;

    manifest.needs_update = manifest.version != read_local_version();
    Ok(manifest)
}

#[tauri::command]
fn install_update(manifest: UpdateManifest) -> Result<String, String> {
    let url = platform_url(&manifest);
    let response = reqwest::blocking::get(url).map_err(|error| error.to_string())?;
    let bytes = response.bytes().map_err(|error| error.to_string())?;
    let update_zip = install_dir()?.join("update.zip");

    fs::write(&update_zip, bytes).map_err(|error| error.to_string())?;
    extract_zip(&update_zip, &install_dir()?)?;
    let _ = fs::remove_file(update_zip);
    write_local_version(&manifest.version)?;

    Ok("Atualizacao instalada".to_string())
}

#[tauri::command]
fn launch_client() -> Result<(), String> {
    let base_dir = install_dir()?;

    #[cfg(target_os = "macos")]
    {
        let app_path = base_dir.join(client_binary_name());
        if !app_path.exists() {
            return Err(format!("Cliente nao encontrado em {}", app_path.display()));
        }

        Command::new("open")
            .arg(app_path)
            .spawn()
            .map_err(|error| error.to_string())?;
    }

    #[cfg(target_os = "windows")]
    {
        let exe_path = base_dir.join(client_binary_name());
        if !exe_path.exists() {
            return Err(format!("Cliente nao encontrado em {}", exe_path.display()));
        }

        Command::new(exe_path)
            .spawn()
            .map_err(|error| error.to_string())?;
    }

    Ok(())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            get_local_version,
            check_updates,
            install_update,
            launch_client
        ])
        .run(tauri::generate_context!())
        .expect("erro ao iniciar o launcher");
}

// backup - backup and restore of the whole setup. The browser does most of the work:
//
// Backup (browser): GET /api/files for the list, GET /fs/<path> for each file, plus
//   /api/state for the strand lengths and strand/house settings; saves one JSON file.
// Restore (browser):
//   1. POST /api/restore-begin    stops all saving and clears /files (old photos)
//   2. POST /api/restore-file?path=/zones.bin   multipart, once per file
//   3. POST /api/restore-finish   reloads zones, paths, looks and scenes from the files
//                                 and allows saving again
//   4. POST /api/strands and /api/state for lengths and strand/house settings
//
// Only these paths can be restored: /zones.bin /paths.bin /presets.bin /scenes.bin
// /programs.bin and /files/<photo>.jpg|.jpeg|.png or /files/layout.json.

const uint32_t RESTORE_TIMEOUT_MS = 10UL * 60 * 1000;  // saving resumes if a restore stalls

File   restoreFile;
bool   restoreOk = false;
String restorePath;
size_t restoreBytes = 0;

bool restorePathAllowed(const String& p) {
  if (p == "/zones.bin" || p == "/paths.bin" || p == "/presets.bin" || p == "/scenes.bin" ||
      p == "/programs.bin" || p == "/patterns.bin") return true;
  if (p.startsWith("/files/")) {
    String n = p.substring(7);
    return n == "layout.json" || (n.length() > 0 && safePhotoName(n) == n);
  }
  return false;
}

// Adds the files of one folder to a JSON list. Root: only the .bin data files.
void listFolder(String& json, const char* dir, bool& first) {
  File d = FFat.open(dir);
  if (!d || !d.isDirectory()) return;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    if (f.isDirectory()) continue;
    String path = f.path();
    if (restorePathAllowed(path)) {
      if (!first) json += ",";
      first = false;
      json += "{\"path\":\"" + path + "\",\"size\":" + String((unsigned)f.size()) + "}";
    }
  }
}

void handleListFiles() {
  if (!storageOk) return sendError("storage not available");
  String json = "{\"files\":[";
  bool first = true;
  listFolder(json, "/", first);
  listFolder(json, FILES_DIR, first);
  json += "]}";
  server.send(200, "application/json", json);
}

void handleRestoreBegin() {
  if (!storageOk) return sendError("storage not available");
  savesBlocked = true;
  savesBlockedMs = millis();
  stopLocate();
  // Remove old photos and layout so only the backup's files remain.
  File d = FFat.open(FILES_DIR);
  if (d && d.isDirectory()) {
    String toDelete[40];
    int n = 0;
    for (File f = d.openNextFile(); f && n < 40; f = d.openNextFile()) {
      if (!f.isDirectory()) toDelete[n++] = f.path();
    }
    d.close();
    for (int i = 0; i < n; i++) FFat.remove(toDelete[i]);
  }
  Serial.println("Restore: started");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleRestoreFileData() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    restorePath = server.arg("path");
    restoreBytes = 0;
    restoreOk = savesBlocked && restorePathAllowed(restorePath);
    if (restoreOk) {
      restoreFile = FFat.open(restorePath, "w");
      restoreOk = (bool)restoreFile;
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (restoreOk) {
      restoreBytes += up.currentSize;
      if (restoreBytes > FILE_MAX_BYTES || restoreFile.write(up.buf, up.currentSize) != up.currentSize) {
        restoreOk = false;
      }
    }
  } else if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
    if (restoreFile) restoreFile.close();
    if (up.status == UPLOAD_FILE_ABORTED) restoreOk = false;
  }
}

void handleRestoreFileDone() {
  if (!savesBlocked) return sendError("restore not started");
  if (!restoreOk) return sendError("could not restore this file");
  Serial.printf("Restore: %s (%u bytes)\n", restorePath.c_str(), (unsigned)restoreBytes);
  server.send(200, "application/json", "{\"ok\":true}");
}

// Reloads everything stored in files, then allows saving again.
void handleRestoreFinish() {
  initZones();
  loadZones();
  initZonePaths();
  loadZonePaths();
  loadPresets();
  loadScenes();
  loadPrograms();
  loadPatterns();
  nudgeSchedule();
  savesBlocked = false;
  markStateChanged();
  Serial.println("Restore: finished");
  sendState();
}

// Called from handleStateSave(): true while saving must wait for a restore.
bool restoreBlocksSaving() {
  if (savesBlocked && millis() - savesBlockedMs > RESTORE_TIMEOUT_MS) {
    savesBlocked = false;
    Serial.println("Restore: timed out, saving allowed again");
  }
  return savesBlocked;
}

void addBackupRoutes() {
  server.on("/api/files", HTTP_GET, handleListFiles);
  server.on("/api/restore-begin", HTTP_POST, handleRestoreBegin);
  server.on("/api/restore-file", HTTP_POST, handleRestoreFileDone, handleRestoreFileData);
  server.on("/api/restore-finish", HTTP_POST, handleRestoreFinish);
  if (storageOk) server.serveStatic("/fs", FFat, "/", "no-cache");
}

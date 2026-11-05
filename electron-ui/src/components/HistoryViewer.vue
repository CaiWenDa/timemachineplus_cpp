<script setup lang="ts">
import { ref } from 'vue'
import { tmb, type FileHistoryItem } from '../api/tmb'

const filepath = ref('')
const loading = ref(false)
const error = ref('')
const history = ref<FileHistoryItem[]>([])
const restoringId = ref<number | null>(null)
const restoreResult = ref('')

async function query() {
  if (!filepath.value.trim()) return
  loading.value = true; error.value = ''; restoreResult.value = ''
  try {
    const r = await tmb.fileHistory(filepath.value.trim())
    history.value = r.history
  } catch (e: any) {
    error.value = e?.message || String(e)
    history.value = []
  } finally {
    loading.value = false
  }
}

async function restore(item: FileHistoryItem) {
  restoringId.value = item.historyId
  restoreResult.value = ''
  try {
    const dest = prompt('恢复到哪个目录？', item.fullBackupPath)
    if (!dest) return
    const r = await tmb.restore(item.historyId, dest)
    restoreResult.value = '✅ 已恢复到 ' + (r as any).restoredTo
  } catch (e: any) {
    restoreResult.value = '❌ ' + (e?.message || String(e))
  } finally {
    restoringId.value = null
  }
}

function humanSize(bytes: number) {
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1024 ** 2) return (bytes / 1024).toFixed(1) + ' KB'
  if (bytes < 1024 ** 3) return (bytes / 1024 / 1024).toFixed(1) + ' MB'
  return (bytes / 1024 / 1024 / 1024).toFixed(2) + ' GB'
}

function humanDate(ms: number) {
  const d = new Date(ms)
  return d.toLocaleString('zh-CN', { hour12: false })
}
</script>

<template>
  <div class="history-viewer">
    <div class="hv-search">
      <label>查看文件历史版本：</label>
      <input
        v-model="filepath"
        placeholder="输入文件完整路径，如 D:/work/report.docx"
        @keyup.enter="query"
      />
      <button class="btn primary" @click="query" :disabled="loading">查询</button>
    </div>

    <div v-if="error" class="hv-error">{{ error }}</div>
    <div v-if="restoreResult" class="hv-result">{{ restoreResult }}</div>

    <div v-if="history.length > 0" class="hv-list">
      <div v-for="item in history" :key="item.historyId" class="hv-item">
        <div class="hv-info">
          <div class="hv-time">🕒 {{ humanDate(item.motifytime) }}</div>
          <div class="hv-meta">
            {{ humanSize(item.filesize) }} · MD5: {{ item.md5.slice(0, 10) }}…
            <span v-if="item.backupBeginTime" class="hv-bt">（备份于 {{ item.backupBeginTime }}）</span>
          </div>
          <div class="hv-path" :title="item.fullBackupPath">📂 {{ item.fullBackupPath }}</div>
        </div>
        <div class="hv-actions">
          <button
            class="btn"
            :disabled="!item.backupFileExists || restoringId === item.historyId"
            @click="restore(item)"
          >
            {{ restoringId === item.historyId ? '恢复中…' : item.backupFileExists ? '↩ 恢复此版本' : '文件缺失' }}
          </button>
        </div>
      </div>
    </div>
    <div v-else-if="!loading && filepath.trim()" class="hv-empty">
      该文件在备份数据库中没有历史版本记录。
    </div>
  </div>
</template>

<style scoped>
.history-viewer { padding: 12px 20px; }
.hv-search { display: flex; gap: 10px; align-items: center; margin-bottom: 14px; }
.hv-search input {
  flex: 1; padding: 7px 11px; border: 1px solid #ddd; border-radius: 6px;
  font-size: 13px;
}
.btn { padding: 6px 14px; border-radius: 6px; border: 1px solid #ddd; background: #fff; cursor: pointer; font-size: 13px; }
.btn.primary { background: #007aff; color: #fff; border-color: #007aff; }
.btn:disabled { opacity: .5; cursor: not-allowed; }

.hv-error { background: #ffe3e3; color: #c00; padding: 8px 12px; border-radius: 6px; margin-bottom: 10px; font-size: 13px; }
.hv-result { background: #e3f7ea; color: #2a7; padding: 8px 12px; border-radius: 6px; margin-bottom: 10px; font-size: 13px; }

.hv-item {
  display: flex; gap: 14px; padding: 12px; margin-bottom: 8px;
  background: #fafafa; border-radius: 8px;
}
.hv-info { flex: 1; min-width: 0; }
.hv-time { font-weight: 600; font-size: 14px; }
.hv-meta { font-size: 12px; color: #666; margin-top: 3px; }
.hv-bt { color: #999; margin-left: 4px; }
.hv-path { font-size: 11px; color: #aaa; margin-top: 4px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.hv-actions { align-self: center; }
.hv-empty { color: #888; font-size: 13px; padding: 20px 0; text-align: center; }
</style>

<script setup lang="ts">
import { ref, watch, nextTick } from 'vue'
import { tmb, type BackupTime, type BackupFile } from '../api/tmb'

// backup：当前选中的时间轴节点；highlightPath：搜索定位后需要高亮的文件
const props = defineProps<{
  backup: BackupTime | null
  highlightPath?: string | null
}>()

const files = ref<BackupFile[]>([])
const loading = ref(false)
const error = ref('')

type RestoreState = 'idle' | 'restoring' | 'ok' | 'fail'
const restoreState = ref<Record<number, RestoreState>>({})
const restoreMsg = ref<Record<number, string>>({})

async function load(id: number) {
  loading.value = true
  error.value = ''
  try {
    const r = await tmb.backupFiles(id)
    files.value = r.files
    await nextTick()
    scrollToHighlight()
  } catch (e: any) {
    error.value = '加载失败：' + (e?.message || String(e))
    files.value = []
  } finally {
    loading.value = false
  }
}

watch(() => props.backup?.id, (id) => {
  restoreState.value = {}
  restoreMsg.value = {}
  if (id == null) files.value = []
  else load(id)
}, { immediate: true })

// 路径归一化后比较（DB 里分隔符可能 / \ 混合）
function norm(p: string) { return p.replace(/\//g, '\\').toLowerCase() }

function isHighlighted(f: BackupFile) {
  return !!props.highlightPath && norm(f.filepath) === norm(props.highlightPath)
}

async function scrollToHighlight() {
  if (!props.highlightPath) return
  await nextTick()
  document.querySelector('.bfl-item.highlight')?.scrollIntoView({ block: 'center', behavior: 'smooth' })
}
watch(() => props.highlightPath, scrollToHighlight)

function basename(p: string) {
  const i = Math.max(p.lastIndexOf('/'), p.lastIndexOf('\\'))
  return i < 0 ? p : p.slice(i + 1)
}

async function restore(f: BackupFile) {
  const ok = confirm(
    `将该文件从当前备份版本恢复到原位置？\n\n文件：${basename(f.filepath)}\n目标：${f.filepath}\n\n同名现有文件将被覆盖。`
  )
  if (!ok) return
  restoreState.value[f.historyId] = 'restoring'
  restoreMsg.value[f.historyId] = ''
  try {
    // 恢复到该文件的原始源路径（预设目标路径）
    const r = await tmb.restore(f.historyId, f.filepath) as any
    restoreState.value[f.historyId] = 'ok'
    restoreMsg.value[f.historyId] = '已恢复到 ' + (r.restoredTo || f.filepath)
  } catch (e: any) {
    restoreState.value[f.historyId] = 'fail'
    restoreMsg.value[f.historyId] = '恢复失败：' + (e?.message || String(e))
  }
}

function humanSize(bytes: number) {
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1024 ** 2) return (bytes / 1024).toFixed(1) + ' KB'
  if (bytes < 1024 ** 3) return (bytes / 1024 / 1024).toFixed(1) + ' MB'
  return (bytes / 1024 / 1024 / 1024).toFixed(2) + ' GB'
}

function humanDate(ms: number) {
  return new Date(ms).toLocaleString('zh-CN', { hour12: false })
}
</script>

<template>
  <div class="bfl">
    <!-- 未选中时间点 -->
    <div v-if="!backup" class="bfl-placeholder">
      <p>👈 点击左侧时间轴上的任意时间点</p>
      <p class="hint">查看该备份版本中包含的所有文件</p>
    </div>

    <template v-else>
      <div class="bfl-header">
        <div class="bfl-title">
          📦 {{ backup.begintime || '未知时间' }} 的备份版本
        </div>
        <span class="bfl-count">{{ files.length }} 个文件</span>
      </div>

      <!-- 加载状态 -->
      <div v-if="loading" class="bfl-loading">
        <span class="spinner"></span> 正在加载文件列表…
      </div>
      <div v-else-if="error" class="bfl-error">{{ error }}</div>
      <div v-else-if="files.length === 0" class="bfl-empty">
        该备份版本没有文件记录。
      </div>

      <div v-else class="bfl-list">
        <div
          v-for="f in files"
          :key="f.historyId"
          class="bfl-item"
          :class="{ highlight: isHighlighted(f) }"
        >
          <div class="bfl-info">
            <div class="bfl-name">📄 {{ basename(f.filepath) }}</div>
            <div class="bfl-path" :title="f.filepath">{{ f.filepath }}</div>
            <div class="bfl-meta">
              {{ humanSize(f.filesize) }} · 修改于 {{ humanDate(f.motifytime) }}
            </div>
            <div
              v-if="restoreState[f.historyId] === 'ok'"
              class="bfl-msg ok"
            >✅ {{ restoreMsg[f.historyId] }}</div>
            <div
              v-else-if="restoreState[f.historyId] === 'fail'"
              class="bfl-msg fail"
            >❌ {{ restoreMsg[f.historyId] }}</div>
          </div>
          <div class="bfl-actions">
            <button
              class="btn"
              :disabled="restoreState[f.historyId] === 'restoring'"
              @click="restore(f)"
            >
              <template v-if="restoreState[f.historyId] === 'restoring'">
                <span class="spinner small"></span> 恢复中…
              </template>
              <template v-else>↩ 恢复</template>
            </button>
          </div>
        </div>
      </div>
    </template>
  </div>
</template>

<style scoped>
.bfl { display: flex; flex-direction: column; height: 100%; }

.bfl-placeholder {
  flex: 1; display: flex; flex-direction: column;
  align-items: center; justify-content: center; color: #999;
}
.bfl-placeholder p { margin: 4px 0; font-size: 15px; }
.bfl-placeholder .hint { font-size: 12px; color: #bbb; }

.bfl-header {
  display: flex; align-items: center; justify-content: space-between;
  padding-bottom: 10px; margin-bottom: 10px; border-bottom: 1px solid #eee;
}
.bfl-title { font-size: 14px; font-weight: 600; }
.bfl-count { font-size: 12px; color: #888; }

.bfl-loading, .bfl-empty, .bfl-error {
  padding: 40px 0; text-align: center; color: #888; font-size: 13px;
}
.bfl-error { color: #c00; }

.bfl-list { flex: 1; overflow-y: auto; }

.bfl-item {
  display: flex; gap: 12px; align-items: flex-start;
  padding: 10px 12px; margin-bottom: 6px;
  background: #fafafa; border-radius: 8px; border: 1px solid transparent;
  transition: background .15s, border-color .15s;
}
.bfl-item:hover { background: #f0f4ff; }
.bfl-item.highlight {
  background: #fff7e0; border-color: #ffd60a;
}

.bfl-info { flex: 1; min-width: 0; }
.bfl-name { font-weight: 600; font-size: 13px; }
.bfl-path {
  font-size: 11px; color: #999; margin-top: 2px;
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.bfl-meta { font-size: 12px; color: #666; margin-top: 4px; }
.bfl-msg { font-size: 12px; margin-top: 6px; }
.bfl-msg.ok { color: #2a7; }
.bfl-msg.fail { color: #c00; }

.bfl-actions { align-self: center; flex-shrink: 0; }

.btn {
  padding: 5px 12px; border-radius: 6px; border: 1px solid #ddd;
  background: #fff; cursor: pointer; font-size: 12px;
  display: inline-flex; align-items: center; gap: 5px;
}
.btn:hover { border-color: #007aff; color: #007aff; }
.btn:disabled { opacity: .6; cursor: not-allowed; }

.spinner {
  display: inline-block; width: 14px; height: 14px;
  border: 2px solid #ddd; border-top-color: #007aff;
  border-radius: 50%; animation: spin .8s linear infinite;
  vertical-align: -2px;
}
.spinner.small { width: 10px; height: 10px; border-width: 1.5px; }
@keyframes spin { to { transform: rotate(360deg); } }
</style>

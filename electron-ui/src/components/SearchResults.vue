<script setup lang="ts">
import type { SearchResult } from '../api/tmb'

// query：当前关键词；results：跨版本搜索结果；searching：加载中
const props = defineProps<{
  query: string
  results: SearchResult[]
  searching: boolean
  error?: string
}>()
const emit = defineEmits<{ (e: 'locate', r: SearchResult): void }>()

function basename(p: string) {
  const i = Math.max(p.lastIndexOf('/'), p.lastIndexOf('\\'))
  return i < 0 ? p : p.slice(i + 1)
}

// 高亮文件名中的关键词片段
function highlightName(p: string) {
  const name = basename(p)
  const kw = props.query.trim().toLowerCase()
  const idx = name.toLowerCase().indexOf(kw)
  if (!kw || idx < 0) return { before: name, hit: '', after: '' }
  return {
    before: name.slice(0, idx),
    hit: name.slice(idx, idx + kw.length),
    after: name.slice(idx + kw.length),
  }
}

function humanSize(bytes: number) {
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1024 ** 2) return (bytes / 1024).toFixed(1) + ' KB'
  if (bytes < 1024 ** 3) return (bytes / 1024 / 1024).toFixed(1) + ' MB'
  return (bytes / 1024 / 1024 / 1024).toFixed(2) + ' GB'
}
</script>

<template>
  <div class="sr">
    <div class="sr-header">
      <span class="sr-title">🔍 「{{ query }}」的搜索结果</span>
      <span class="sr-count">{{ searching ? '搜索中…' : `${results.length} 条匹配` }}</span>
    </div>

    <div v-if="searching" class="sr-status">
      <span class="spinner"></span> 正在搜索所有备份版本…
    </div>
    <div v-else-if="error" class="sr-status err">搜索失败：{{ error }}</div>
    <div v-else-if="!results.length" class="sr-status">
      所有备份版本中都没有找到文件名包含「{{ query }}」的文件。
    </div>

    <div v-else class="sr-list">
      <div
        v-for="r in results"
        :key="r.historyId"
        class="sr-item"
        @click="emit('locate', r)"
        title="点击定位到该文件所在的备份版本"
      >
        <div class="sr-info">
          <div class="sr-name">
            📄 {{ highlightName(r.filepath).before }}<mark>{{ highlightName(r.filepath).hit }}</mark>{{ highlightName(r.filepath).after }}
          </div>
          <div class="sr-path">{{ r.filepath }}</div>
        </div>
        <div class="sr-side">
          <span class="sr-version">🕒 {{ r.backupBeginTime || `备份 #${r.backupId}` }}</span>
          <span class="sr-size">{{ humanSize(r.filesize) }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.sr { display: flex; flex-direction: column; height: 100%; }

.sr-header {
  display: flex; align-items: center; justify-content: space-between;
  padding-bottom: 10px; margin-bottom: 10px; border-bottom: 1px solid #eee;
}
.sr-title { font-size: 14px; font-weight: 600; }
.sr-count { font-size: 12px; color: #888; }

.sr-status { padding: 40px 0; text-align: center; color: #888; font-size: 13px; }
.sr-status.err { color: #c00; }

.sr-list { flex: 1; overflow-y: auto; }

.sr-item {
  display: flex; gap: 12px; align-items: center;
  padding: 10px 12px; margin-bottom: 6px;
  background: #fafafa; border-radius: 8px; cursor: pointer;
  transition: background .15s;
}
.sr-item:hover { background: #e8f0ff; }

.sr-info { flex: 1; min-width: 0; }
.sr-name { font-weight: 600; font-size: 13px; }
.sr-name mark { background: #ffd60a; border-radius: 2px; padding: 0 1px; }
.sr-path {
  font-size: 11px; color: #999; margin-top: 2px;
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}

.sr-side { flex-shrink: 0; text-align: right; }
.sr-version {
  display: block; font-size: 11px; color: #007aff;
  background: #e8f0ff; padding: 2px 8px; border-radius: 10px;
}
.sr-size { display: block; font-size: 11px; color: #999; margin-top: 4px; }

.spinner {
  display: inline-block; width: 14px; height: 14px;
  border: 2px solid #ddd; border-top-color: #007aff;
  border-radius: 50%; animation: spin .8s linear infinite;
  vertical-align: -2px;
}
@keyframes spin { to { transform: rotate(360deg); } }
</style>

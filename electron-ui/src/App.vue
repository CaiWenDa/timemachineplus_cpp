<script setup lang="ts">
import { ref, watch, computed, onMounted } from 'vue'
import Timeline from './components/Timeline.vue'
import BackupFileList from './components/BackupFileList.vue'
import SearchResults from './components/SearchResults.vue'
import SettingsModal from './components/SettingsModal.vue'
import { tmb, type BackupTime, type SearchResult } from './api/tmb'

// ---- 后端连接状态 ----
const health = ref('')
const status = ref<'loading' | 'ok' | 'fail'>('loading')

async function checkHealth() {
  try {
    const r = await tmb.health()
    health.value = r.status; status.value = 'ok'
  } catch {
    health.value = 'C++ 后端未启动'; status.value = 'fail'
  }
}

// ---- 时间轴选中 + 右侧文件面板 ----
const selectedBackup = ref<BackupTime | null>(null)
const highlightPath = ref<string | null>(null)

function onTimelineSelect(b: BackupTime) {
  selectedBackup.value = b
  highlightPath.value = null
  // 手动浏览时间点时退出搜索模式
  searchQuery.value = ''
  searchResults.value = []
}

// ---- 全局搜索（按文件名关键词，跨所有备份版本）----
const searchQuery = ref('')
const searchResults = ref<SearchResult[]>([])
const searching = ref(false)
const searchError = ref('')
let searchTimer: ReturnType<typeof setTimeout> | null = null

watch(searchQuery, (q) => {
  if (searchTimer) clearTimeout(searchTimer)
  const kw = q.trim()
  if (!kw) {
    searchResults.value = []
    searching.value = false
    searchError.value = ''
    return
  }
  // 300ms 防抖，避免输入过程频繁请求
  searchTimer = setTimeout(async () => {
    searching.value = true
    searchError.value = ''
    try {
      const r = await tmb.search(kw)
      searchResults.value = r.results
    } catch (e: any) {
      searchError.value = e?.message || String(e)
      searchResults.value = []
    } finally {
      searching.value = false
    }
  }, 300)
})

// 点击搜索结果 → 跨版本定位：选中对应时间轴节点并高亮该文件
function onLocate(r: SearchResult) {
  selectedBackup.value = {
    id: r.backupId,
    begintime: r.backupBeginTime,
    endtime: null,
    filecopycount: null,
    datacopycount: null,
  }
  highlightPath.value = r.filepath
  searchQuery.value = ''
  searchResults.value = []
}

// ---- 设置页面（模态）----
const settingsOpen = ref(false)

const searchMode = computed(() => searchQuery.value.trim().length > 0)

onMounted(checkHealth)
</script>

<template>
  <div class="app">
    <!-- 顶部：标题 + 全局搜索 + 状态 + 设置入口 -->
    <div class="app-top">
      <span class="app-title">🕓 Time Machine+</span>

      <div class="app-search">
        <input
          v-model="searchQuery"
          class="search-input"
          type="search"
          placeholder="🔍 输入文件名关键词，搜索所有备份版本…"
          @keydown.esc="searchQuery = ''"
        />
      </div>

      <span :class="['app-health', status]">● {{ health }}</span>
      <button v-if="status === 'fail'" class="btn tiny" @click="checkHealth">重试连接</button>
      <button class="btn settings-btn" @click="settingsOpen = true">⚙️ 设置</button>
    </div>

    <div class="app-body">
      <!-- 左侧：时间轴 -->
      <aside class="app-left">
        <Timeline :selected-id="selectedBackup?.id ?? null" @select="onTimelineSelect" />
      </aside>

      <!-- 右侧：搜索结果 或 当前版本的文件列表 -->
      <main class="app-right">
        <section class="panel flex">
          <SearchResults
            v-if="searchMode"
            :query="searchQuery.trim()"
            :results="searchResults"
            :searching="searching"
            :error="searchError"
            @locate="onLocate"
          />
          <BackupFileList
            v-else
            :backup="selectedBackup"
            :highlight-path="highlightPath"
          />
        </section>
      </main>
    </div>

    <!-- 独立设置页面（模态窗口） -->
    <SettingsModal :open="settingsOpen" @close="settingsOpen = false" />
  </div>
</template>

<style>
* { box-sizing: border-box; }
body { margin: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: #f5f5f7; color: #333; }
#app { height: 100vh; display: flex; flex-direction: column; }
ul { margin: 0; padding: 0; list-style: none; }
code { font-family: ui-monospace, SFMono-Regular, Consolas, monospace; font-size: 12px; }
button { font-family: inherit; }
input { font-family: inherit; outline: none; }
input:focus { border-color: #007aff; }
</style>

<style scoped>
.app { display: flex; flex-direction: column; height: 100vh; background: #f5f5f7; }

.app-top {
  display: flex; align-items: center; gap: 12px;
  padding: 10px 20px; background: #fff; border-bottom: 1px solid #e5e5ea;
  -webkit-app-region: drag;
}
.app-title { font-weight: 600; font-size: 15px; flex-shrink: 0; }

.app-search { flex: 1; display: flex; justify-content: center; -webkit-app-region: no-drag; }
.search-input {
  width: min(520px, 100%);
  padding: 7px 14px; font-size: 13px;
  border: 1px solid #ddd; border-radius: 16px;
  background: #f5f5f7;
  transition: border-color .15s, background .15s;
}
.search-input:focus { border-color: #007aff; background: #fff; }

.app-health { font-size: 12px; color: #888; flex-shrink: 0; -webkit-app-region: no-drag; }
.app-health.ok { color: #2a7; }
.app-health.fail { color: #c00; }
.app-top .btn { -webkit-app-region: no-drag; flex-shrink: 0; }

.settings-btn { font-weight: 600; }
.settings-btn:hover { border-color: #007aff; color: #007aff; }

.app-body { flex: 1; display: grid; grid-template-columns: 320px 1fr; overflow: hidden; }

.app-left {
  border-right: 1px solid #e5e5ea; background: #fff; overflow-y: auto;
}

.app-right { overflow: hidden; padding: 16px; display: flex; flex-direction: column; }

.panel {
  background: #fff; border-radius: 10px; padding: 14px 16px;
  box-shadow: 0 1px 3px rgba(0,0,0,.04);
}
.panel.flex { flex: 1; display: flex; flex-direction: column; overflow: hidden; }

.btn { padding: 6px 14px; border-radius: 6px; border: 1px solid #ddd; background: #fff; cursor: pointer; font-size: 13px; }
.btn.tiny { padding: 3px 10px; font-size: 12px; }

/* Timeline 标题栏重设：App.vue 已有顶栏，隐藏 Timeline 自带 header */
:deep(.tl-header) { display: none; }

/* 响应式：窄窗口时堆叠为单列 */
@media (max-width: 860px) {
  .app-body { grid-template-columns: 1fr; grid-template-rows: minmax(180px, 38%) 1fr; }
  .app-left { border-right: none; border-bottom: 1px solid #e5e5ea; }
}
</style>

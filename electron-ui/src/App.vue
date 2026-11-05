<script setup lang="ts">
import { ref, onMounted } from 'vue'
import Timeline from './components/Timeline.vue'
import HistoryViewer from './components/HistoryViewer.vue'
import { tmb, type Source } from './api/tmb'

const sources = ref<Source[]>([])
const newPath = ref('')
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

async function refreshSources() {
  const r = await tmb.sources()
  sources.value = r.sources
}

async function addSource() {
  if (!newPath.value.trim()) return
  await tmb.addSource(newPath.value.trim())
  newPath.value = ''
  await refreshSources()
}

async function deleteSource(id: number) {
  await tmb.deleteSource(id)
  await refreshSources()
}

onMounted(() => { checkHealth(); refreshSources() })
</script>

<template>
  <div class="app">
    <!-- 顶部状态栏 -->
    <div class="app-top">
      <span class="app-title">Time Machine+</span>
      <span :class="['app-health', status]">⚙ {{ health }}</span>
      <button class="btn tiny" @click="checkHealth">重试连接</button>
    </div>

    <div class="app-body">
      <!-- 左侧：时间轴 + 源路径管理 -->
      <aside class="app-left">
        <Timeline />
      </aside>

      <!-- 右侧：历史版本查看 + 恢复 -->
      <main class="app-right">
        <section class="panel">
          <div class="panel-title">📂 源路径</div>
          <div class="panel-row">
            <input v-model="newPath" placeholder="添加备份源路径，如 D:/work" @keyup.enter="addSource"/>
            <button class="btn primary" @click="addSource">+ 添加</button>
          </div>
          <ul class="source-list">
            <li v-for="s in sources" :key="s.id">
              <code>{{ s.path }}</code>
              <button class="btn tiny danger" @click="deleteSource(s.id)">删除</button>
            </li>
            <li v-if="sources.length === 0" class="empty">暂无源路径</li>
          </ul>
        </section>

        <section class="panel flex">
          <div class="panel-title">🕓 历史版本回溯</div>
          <HistoryViewer />
        </section>
      </main>
    </div>
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
.app-title { font-weight: 600; font-size: 15px; }
.app-health { font-size: 12px; color: #888; margin-left: auto; }
.app-health.ok { color: #2a7; }
.app-health.fail { color: #c00; }
.app-top .btn { -webkit-app-region: no-drag; }

.app-body { flex: 1; display: grid; grid-template-columns: 340px 1fr; overflow: hidden; }

.app-left {
  border-right: 1px solid #e5e5ea; background: #fff; overflow-y: auto;
}

.app-right { overflow-y: auto; padding: 16px; display: flex; flex-direction: column; gap: 16px; }

.panel {
  background: #fff; border-radius: 10px; padding: 14px 16px;
  box-shadow: 0 1px 3px rgba(0,0,0,.04);
}
.panel.flex { flex: 1; display: flex; flex-direction: column; }
.panel-title { font-size: 14px; font-weight: 600; margin-bottom: 12px; }
.panel-row { display: flex; gap: 8px; margin-bottom: 10px; }
.panel-row input { flex: 1; padding: 7px 11px; border: 1px solid #ddd; border-radius: 6px; font-size: 13px; }

.source-list li {
  display: flex; justify-content: space-between; align-items: center;
  padding: 8px 10px; background: #f9f9f9; border-radius: 6px; margin-bottom: 6px;
}
.source-list li.empty { color: #aaa; justify-content: center; font-size: 13px; background: transparent; }

.btn { padding: 6px 14px; border-radius: 6px; border: 1px solid #ddd; background: #fff; cursor: pointer; font-size: 13px; }
.btn.primary { background: #007aff; color: #fff; border-color: #007aff; }
.btn.primary:hover { background: #0066d6; }
.btn.tiny { padding: 3px 10px; font-size: 12px; }
.btn.danger { color: #c00; border-color: #fbb; }
.btn.danger:hover { background: #ffeeee; }

/* Timeline 标题栏重设：App.vue 已经有 top bar，把 Timeline 自己的 header 隐藏 */
:deep(.tl-header) { display: none; }
</style>

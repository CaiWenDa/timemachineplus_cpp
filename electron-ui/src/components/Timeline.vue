<script setup lang="ts">
import { ref, onMounted, computed, watch, nextTick } from 'vue'
import { tmb, type BackupTime } from '../api/tmb'

// selectedId 由父组件控制（支持搜索结果跨版本定位时外部传入）
const props = defineProps<{ selectedId?: number | null }>()
const emit = defineEmits<{ (e: 'select', b: BackupTime): void }>()

const backups = ref<BackupTime[]>([])
const loading = ref(true)
const selectedBackupId = ref<number | null>(props.selectedId ?? null)

// 外部（搜索结果定位）改变选中节点时，同步高亮并滚动到可见位置
watch(() => props.selectedId, async (v) => {
  if (v == null) return
  selectedBackupId.value = v
  await nextTick()
  document.getElementById(`tl-dot-${v}`)?.scrollIntoView({ block: 'nearest', behavior: 'smooth' })
})

async function load() {
  loading.value = true
  try {
    const r = await tmb.backups()
    backups.value = r.backups
  } catch (e) {
    console.error(e)
  } finally {
    loading.value = false
  }
}

function select(b: BackupTime) {
  selectedBackupId.value = b.id
  emit('select', b)
}

onMounted(load)

// 苹果 Time Machine 风格：把时间点按日期分组
const grouped = computed(() => {
  const map = new Map<string, BackupTime[]>()
  for (const b of backups.value) {
    const day = (b.begintime || '未知')
      .slice(0, 10)  // 取 YYYY-MM-DD
    if (!map.has(day)) map.set(day, [])
    map.get(day)!.push(b)
  }
  return Array.from(map.entries())
})

function humanSize(bytes: number | null) {
  if (bytes == null) return '—'
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1024 ** 2) return (bytes / 1024).toFixed(1) + ' KB'
  if (bytes < 1024 ** 3) return (bytes / 1024 / 1024).toFixed(1) + ' MB'
  return (bytes / 1024 / 1024 / 1024).toFixed(2) + ' GB'
}
</script>

<template>
  <div class="timeline">
    <header class="tl-header">
      <h1>⏱ Time Machine+</h1>
      <p class="tl-sub">你的文件时光机</p>
    </header>

    <section class="tl-controls">
      <button class="btn primary" @click="load" :disabled="loading">
        {{ loading ? '加载中…' : '刷新' }}
      </button>
      <button class="btn" @click="tmb.triggerBackup(); load()">
        📸 立即备份
      </button>
      <span class="tl-count">{{ backups.length }} 个备份点</span>
    </section>

    <div v-if="backups.length === 0 && !loading" class="tl-empty">
      <p>还没有备份历史。</p>
      <p class="hint">添加源路径后点击「立即备份」开始吧。</p>
    </div>

    <div v-else class="tl-scroll">
      <div v-for="[day, list] in grouped" :key="day" class="tl-day">
        <div class="tl-day-label">{{ day }}</div>
        <div class="tl-day-items">
          <div
            v-for="b in list"
            :key="b.id"
            :id="`tl-dot-${b.id}`"
            class="tl-dot"
            :class="{ active: selectedBackupId === b.id, pending: !b.endtime }"
            @click="select(b)"
          >
            <span class="tl-time">{{ (b.begintime || '').slice(11, 16) }}</span>
            <span class="tl-files">{{ b.filecopycount ?? 0 }} 个文件</span>
            <span class="tl-size">{{ humanSize(b.datacopycount) }}</span>
            <span v-if="!b.endtime" class="tl-badge">进行中</span>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.timeline {
  height: 100%;
  display: flex;
  flex-direction: column;
}
.tl-header { padding: 16px 20px 8px; }
.tl-header h1 { margin: 0; font-size: 20px; font-weight: 600; }
.tl-sub { margin: 2px 0 0; font-size: 12px; color: #888; }

.tl-controls {
  display: flex; align-items: center; gap: 10px;
  padding: 8px 20px 12px; border-bottom: 1px solid #eee;
}
.tl-count { margin-left: auto; font-size: 12px; color: #888; }

.btn {
  padding: 6px 14px; border-radius: 6px; border: 1px solid #ddd;
  background: #fff; cursor: pointer; font-size: 13px;
}
.btn:hover { border-color: #aaa; }
.btn.primary { background: #007aff; color: #fff; border-color: #007aff; }
.btn.primary:hover { background: #0066d6; }
.btn:disabled { opacity: .5; cursor: not-allowed; }

.tl-empty {
  padding: 60px 20px; text-align: center; color: #999;
}
.tl-empty .hint { font-size: 12px; }

.tl-scroll {
  flex: 1; overflow-y: auto; padding: 12px 16px;
}
.tl-day { margin-bottom: 16px; }
.tl-day-label {
  font-size: 11px; font-weight: 600; text-transform: uppercase;
  color: #888; padding: 4px 6px;
}
.tl-day-items {
  position: relative; padding-left: 20px;
  border-left: 2px solid #e5e5ea;
}
.tl-dot {
  position: relative; padding: 10px 14px; margin: 4px 0;
  background: #fafafa; border-radius: 8px; cursor: pointer;
  transition: background .15s;
}
.tl-dot::before {
  content: ''; position: absolute; left: -28px; top: 14px;
  width: 12px; height: 12px; border-radius: 50%;
  background: #007aff; border: 2px solid #fff;
  box-shadow: 0 0 0 2px #007aff;
}
.tl-dot:hover { background: #f0f4ff; }
.tl-dot.active { background: #e8f0ff; }
.tl-dot.pending::before {
  background: #ff9500; box-shadow: 0 0 0 2px #ff9500;
}
.tl-time { font-weight: 600; font-size: 13px; }
.tl-files { margin-left: 10px; font-size: 12px; color: #666; }
.tl-size { margin-left: 10px; font-size: 12px; color: #999; }
.tl-badge {
  float: right; font-size: 10px; color: #fff;
  background: #ff9500; padding: 2px 8px; border-radius: 10px;
}
</style>

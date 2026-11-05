<script setup lang="ts">
import { ref, watch } from 'vue'
import { tmb, type Source, type Target } from '../api/tmb'

const props = defineProps<{ open: boolean }>()
const emit = defineEmits<{ (e: 'close'): void }>()

const sources = ref<Source[]>([])
const targets = ref<Target[]>([])
const newSource = ref('')
const newTarget = ref('')
const loading = ref(false)

// 表单验证错误 + 操作结果提示（保存确认 / 变更提示）
const sourceErr = ref('')
const targetErr = ref('')
const notice = ref<{ type: 'ok' | 'err'; text: string } | null>(null)
let noticeTimer: ReturnType<typeof setTimeout> | null = null

function showNotice(type: 'ok' | 'err', text: string) {
  notice.value = { type, text }
  if (noticeTimer) clearTimeout(noticeTimer)
  noticeTimer = setTimeout(() => (notice.value = null), 3500)
}

function validPath(p: string): string {
  const v = p.trim()
  if (!v) return '路径不能为空'
  // 桌面端绝对路径：Windows 盘符 / UNC / Unix 根路径
  if (!/^[a-zA-Z]:[\\/]/.test(v) && !v.startsWith('\\\\') && !v.startsWith('/')) {
    return '请输入绝对路径，如 D:/work 或 D:\\work'
  }
  return ''
}

async function refresh() {
  loading.value = true
  try {
    const [s, t] = await Promise.all([tmb.sources(), tmb.targets()])
    sources.value = s.sources
    targets.value = t.targets
  } catch (e: any) {
    showNotice('err', '加载配置失败：' + (e?.message || String(e)))
  } finally {
    loading.value = false
  }
}

// 打开时加载最新配置，并注册 Esc 关闭
watch(() => props.open, (v) => {
  if (v) {
    refresh()
    window.addEventListener('keydown', onKeydown)
  } else {
    window.removeEventListener('keydown', onKeydown)
  }
})

async function addSource() {
  sourceErr.value = validPath(newSource.value)
  if (sourceErr.value) return
  const p = newSource.value.trim()
  if (sources.value.some(s => s.path === p)) {
    sourceErr.value = '该源路径已存在'
    return
  }
  try {
    await tmb.addSource(p)
    newSource.value = ''
    await refresh()
    showNotice('ok', '✅ 源路径已保存，即时生效无需重启')
  } catch (e: any) {
    showNotice('err', '保存失败：' + (e?.message || String(e)))
  }
}

async function removeSource(s: Source) {
  if (!confirm(`确定删除该备份源？\n${s.path}\n\n删除后该路径将不再参与后续备份。`)) return
  try {
    await tmb.deleteSource(s.id)
    await refresh()
    showNotice('ok', '✅ 源路径已删除，即时生效')
  } catch (e: any) {
    showNotice('err', '删除失败：' + (e?.message || String(e)))
  }
}

async function addTarget() {
  targetErr.value = validPath(newTarget.value)
  if (targetErr.value) return
  const p = newTarget.value.trim()
  if (targets.value.some(t => t.path === p)) {
    targetErr.value = '该目标路径已存在'
    return
  }
  try {
    await tmb.addTarget(p)
    newTarget.value = ''
    await refresh()
    showNotice('ok', '✅ 目标路径已保存，即时生效无需重启')
  } catch (e: any) {
    showNotice('err', '保存失败：' + (e?.message || String(e)))
  }
}

async function removeTarget(t: Target) {
  if (!confirm(`确定删除该备份目标？\n${t.path}\n\n已有备份数据不会被删除，仅移除配置。`)) return
  try {
    await tmb.deleteTarget(t.id)
    await refresh()
    showNotice('ok', '✅ 目标路径已删除，即时生效')
  } catch (e: any) {
    showNotice('err', '删除失败：' + (e?.message || String(e)))
  }
}

function humanSpace(bytes: number) {
  if (bytes < 1024 ** 3) return (bytes / 1024 / 1024).toFixed(0) + ' MB'
  return (bytes / 1024 / 1024 / 1024).toFixed(1) + ' GB'
}

function onOverlayClick(e: MouseEvent) {
  if (e.target === e.currentTarget) emit('close')
}
function onKeydown(e: KeyboardEvent) {
  if (e.key === 'Escape') emit('close')
}
</script>

<template>
  <Teleport to="body">
    <div
      v-if="open"
      class="st-overlay"
      @click="onOverlayClick"
    >
      <div class="st-modal" role="dialog" aria-label="设置">
        <header class="st-header">
          <span class="st-title">⚙️ 设置</span>
          <button class="st-close" @click="emit('close')" title="关闭">✕</button>
        </header>

        <!-- 变更提示条 -->
        <transition name="fade">
          <div v-if="notice" :class="['st-notice', notice.type]">{{ notice.text }}</div>
        </transition>

        <div class="st-body">
          <!-- 源路径配置 -->
          <section class="st-section">
            <div class="st-section-title">📂 备份源路径</div>
            <p class="st-desc">这些目录中的文件会被定期备份。</p>
            <div class="st-row">
              <input
                v-model="newSource"
                placeholder="如 D:/work 或 D:\work"
                :class="{ invalid: sourceErr }"
                @input="sourceErr = ''"
                @keyup.enter="addSource"
              />
              <button class="btn primary" @click="addSource">+ 添加</button>
            </div>
            <div v-if="sourceErr" class="st-field-err">{{ sourceErr }}</div>
            <ul class="st-list">
              <li v-for="s in sources" :key="s.id">
                <code>{{ s.path }}</code>
                <button class="btn tiny danger" @click="removeSource(s)">删除</button>
              </li>
              <li v-if="!sources.length && !loading" class="empty">暂无源路径</li>
            </ul>
          </section>

          <!-- 目标路径配置 -->
          <section class="st-section">
            <div class="st-section-title">💾 备份目标路径</div>
            <p class="st-desc">备份数据将写入剩余空间最大的目标目录。</p>
            <div class="st-row">
              <input
                v-model="newTarget"
                placeholder="如 E:/backups"
                :class="{ invalid: targetErr }"
                @input="targetErr = ''"
                @keyup.enter="addTarget"
              />
              <button class="btn primary" @click="addTarget">+ 添加</button>
            </div>
            <div v-if="targetErr" class="st-field-err">{{ targetErr }}</div>
            <ul class="st-list">
              <li v-for="t in targets" :key="t.id">
                <code>{{ t.path }}</code>
                <span class="st-space">剩余 {{ humanSpace(t.spaceRemain) }}</span>
                <button class="btn tiny danger" @click="removeTarget(t)">删除</button>
              </li>
              <li v-if="!targets.length && !loading" class="empty">暂无目标路径</li>
            </ul>
          </section>
        </div>

        <footer class="st-footer">
          所有配置变更即时生效，无需重启应用。
          <button class="btn primary" @click="emit('close')">完成</button>
        </footer>
      </div>
    </div>
  </Teleport>
</template>

<style scoped>
.st-overlay {
  position: fixed; inset: 0; background: rgba(0, 0, 0, .35);
  display: flex; align-items: center; justify-content: center;
  z-index: 1000; outline: none;
}
.st-modal {
  width: min(640px, 92vw); max-height: 86vh;
  background: #fff; border-radius: 12px;
  box-shadow: 0 20px 60px rgba(0, 0, 0, .25);
  display: flex; flex-direction: column; overflow: hidden;
}

.st-header {
  display: flex; align-items: center; justify-content: space-between;
  padding: 14px 20px; border-bottom: 1px solid #eee;
}
.st-title { font-size: 15px; font-weight: 600; }
.st-close {
  border: none; background: none; font-size: 15px; cursor: pointer;
  color: #888; padding: 4px 8px; border-radius: 6px;
}
.st-close:hover { background: #f0f0f0; color: #333; }

.st-notice {
  margin: 10px 20px 0; padding: 8px 12px; border-radius: 6px; font-size: 13px;
}
.st-notice.ok { background: #e3f7ea; color: #2a7; }
.st-notice.err { background: #ffe3e3; color: #c00; }
.fade-enter-active, .fade-leave-active { transition: opacity .25s; }
.fade-enter-from, .fade-leave-to { opacity: 0; }

.st-body { padding: 16px 20px; overflow-y: auto; }
.st-section { margin-bottom: 22px; }
.st-section-title { font-size: 14px; font-weight: 600; }
.st-desc { font-size: 12px; color: #999; margin: 4px 0 10px; }

.st-row { display: flex; gap: 8px; }
.st-row input {
  flex: 1; padding: 8px 12px; border: 1px solid #ddd; border-radius: 6px; font-size: 13px;
}
.st-row input:focus { border-color: #007aff; }
.st-row input.invalid { border-color: #e33; }
.st-field-err { color: #e33; font-size: 12px; margin-top: 5px; }

.st-list { margin-top: 10px; }
.st-list li {
  display: flex; align-items: center; gap: 10px;
  padding: 8px 10px; background: #f9f9f9; border-radius: 6px; margin-bottom: 6px;
}
.st-list li code { flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.st-list li.empty { color: #aaa; justify-content: center; font-size: 13px; background: transparent; }
.st-space { font-size: 11px; color: #999; flex-shrink: 0; }

.st-footer {
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 20px; border-top: 1px solid #eee;
  font-size: 12px; color: #999;
}

.btn { padding: 6px 14px; border-radius: 6px; border: 1px solid #ddd; background: #fff; cursor: pointer; font-size: 13px; }
.btn.primary { background: #007aff; color: #fff; border-color: #007aff; }
.btn.primary:hover { background: #0066d6; }
.btn.tiny { padding: 3px 10px; font-size: 12px; }
.btn.danger { color: #c00; border-color: #fbb; }
.btn.danger:hover { background: #ffeeee; }
</style>

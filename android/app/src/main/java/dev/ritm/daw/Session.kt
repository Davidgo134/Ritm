package dev.ritm.daw

import android.content.Context
import android.net.Uri
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import java.io.File
import kotlin.concurrent.thread
import kotlin.math.ceil
import kotlin.math.max
import kotlin.math.min

data class N(val tick: Int, val len: Int, val key: Int, val vel: Int = 100)

class Session {
    var ppq by mutableIntStateOf(96)
    var bpm by mutableDoubleStateOf(120.0)
    var bars by mutableIntStateOf(4)
    var lenSteps by mutableIntStateOf(1)
    var status by mutableStateOf("")
    var position by mutableDoubleStateOf(0.0)
    var playing by mutableStateOf(false)
    val notes = mutableStateListOf<N>()

    val stepTicks: Int get() = max(1, ppq / 4)

    fun keyRange(): Pair<Int, Int> {
        val lo = min(48, (notes.minOfOrNull { it.key } ?: 60) - 2)
        val hi = max(83, (notes.maxOfOrNull { it.key } ?: 60) + 2)
        return lo to hi
    }

    fun push() {
        val arr = IntArray(notes.size * 4)
        for ((i, n) in notes.withIndex()) {
            arr[i * 4] = n.tick
            arr[i * 4 + 1] = n.len
            arr[i * 4 + 2] = n.key
            arr[i * 4 + 3] = n.vel
        }
        Native.setNotes(arr, bpm, ppq, bars * 4.0 * ppq)
    }

    fun loadDemo() {
        ppq = 96
        bpm = 120.0
        bars = 2
        notes.clear()
        val keys = intArrayOf(60, 63, 67, 70, 72, 70, 67, 63)
        for (i in 0 until 16) {
            notes.add(N(i * 48, 40, keys[i % keys.size]))
        }
        notes.add(N(0, 192, 48, 110))
        notes.add(N(192, 192, 51, 110))
        Native.setBpm(bpm)
        push()
    }

    fun importFrom(ctx: Context, uri: Uri) {
        status = "Importing..."
        thread {
            try {
                val f = File(ctx.cacheDir, "import.bin")
                ctx.contentResolver.openInputStream(uri).use { input ->
                    if (input == null) {
                        status = "Cannot open file"
                        return@thread
                    }
                    f.outputStream().use { input.copyTo(it) }
                }
                val r = Native.importProject(f.absolutePath)
                if (!r.startsWith("OK:")) {
                    status = r.removePrefix("ERR:")
                    return@thread
                }
                val flat = Native.importedNotes()
                val newPpq = Native.importedPpq()
                val newNotes = ArrayList<N>(flat.size / 4)
                var i = 0
                var end = 0
                while (i + 3 < flat.size) {
                    newNotes.add(N(flat[i], max(1, flat[i + 1]), flat[i + 2], flat[i + 3]))
                    end = max(end, flat[i] + flat[i + 1])
                    i += 4
                }
                ppq = newPpq
                bpm = Native.importedBpm()
                bars = max(1, ceil(end / (4.0 * newPpq)).toInt())
                notes.clear()
                notes.addAll(newNotes)
                Native.setBpm(bpm)
                push()
                status = r.removePrefix("OK:")
            } catch (e: Exception) {
                status = "Import failed"
            }
        }
    }
}

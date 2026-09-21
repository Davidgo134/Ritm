package dev.ritm.daw

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.roundToInt

class MainActivity : ComponentActivity() {
    private val session = Session()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val ok = Native.start()
        session.loadDemo()
        if (!ok) session.status = "Audio output unavailable"
        setContent { App(session) }
    }

    override fun onDestroy() {
        if (isFinishing) Native.shutdown()
        super.onDestroy()
    }
}

@Composable
fun App(s: Session) {
    MaterialTheme(colorScheme = darkColorScheme()) {
        Surface(Modifier.fillMaxSize()) {
            Column(Modifier.fillMaxSize().statusBarsPadding().navigationBarsPadding()) {
                Transport(s)
                PianoRoll(s, Modifier.weight(1f))
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun Transport(s: Session) {
    val ctx = LocalContext.current
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) s.importFrom(ctx, uri)
    }
    LaunchedEffect(Unit) {
        while (true) {
            s.playing = Native.isPlaying()
            s.position = Native.position()
            delay(16)
        }
    }
    Column(Modifier.padding(8.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Button(onClick = { if (s.playing) Native.pause() else Native.play() }) {
                Text(if (s.playing) "Pause" else "Play")
            }
            OutlinedButton(onClick = { Native.stopAndRewind() }, modifier = Modifier.padding(start = 8.dp)) {
                Text("Stop")
            }
            Text("${s.bpm.roundToInt()} BPM", modifier = Modifier.padding(start = 12.dp))
            Slider(
                value = s.bpm.toFloat(),
                onValueChange = {
                    s.bpm = it.toDouble()
                    Native.setBpm(s.bpm)
                },
                valueRange = 60f..200f,
                modifier = Modifier.weight(1f).padding(start = 8.dp)
            )
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedButton(onClick = { launcher.launch(arrayOf("*/*")) }) { Text("Open") }
            OutlinedButton(
                onClick = {
                    s.notes.clear()
                    s.push()
                },
                modifier = Modifier.padding(start = 8.dp)
            ) { Text("Clear") }
            OutlinedButton(
                onClick = {
                    if (s.bars > 1) {
                        s.bars--
                        s.push()
                    }
                },
                modifier = Modifier.padding(start = 8.dp)
            ) { Text("-") }
            Text("${s.bars}", modifier = Modifier.padding(horizontal = 8.dp))
            OutlinedButton(onClick = {
                if (s.bars < 64) {
                    s.bars++
                    s.push()
                }
            }) { Text("+") }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            for ((label, steps) in listOf("1/16" to 1, "1/8" to 2, "1/4" to 4, "1/2" to 8, "1" to 16)) {
                FilterChip(
                    selected = s.lenSteps == steps,
                    onClick = { s.lenSteps = steps },
                    label = { Text(label) },
                    modifier = Modifier.padding(end = 4.dp)
                )
            }
        }
        if (s.status.isNotEmpty()) Text(s.status)
    }
}

private val blackKeys = setOf(1, 3, 6, 8, 10)

@Composable
fun PianoRoll(s: Session, modifier: Modifier = Modifier) {
    val density = LocalDensity.current
    val rowPx = with(density) { 22.dp.toPx() }
    val beatPx = with(density) { 64.dp.toPx() }
    val (lo, hi) = s.keyRange()
    val rows = hi - lo + 1
    val totalBeats = s.bars * 4
    val widthDp = with(density) { (beatPx * totalBeats).toDp() }
    val heightDp = with(density) { (rowPx * rows).toDp() }
    val hs = rememberScrollState()
    val vs = rememberScrollState()
    LaunchedEffect(Unit) { vs.scrollTo(vs.maxValue / 3) }

    Box(modifier.background(Color(0xFF15151B)).verticalScroll(vs).horizontalScroll(hs)) {
        Canvas(
            Modifier
                .size(widthDp, heightDp)
                .pointerInput(Unit) {
                    detectTapGestures { off ->
                        val (l, h) = s.keyRange()
                        val key = h - floor(off.y / rowPx).toInt()
                        val raw = off.x / beatPx * s.ppq
                        val step = s.stepTicks
                        val t = floor(raw / step).toInt() * step
                        val hit = s.notes.indexOfFirst { it.key == key && t >= it.tick && t < it.tick + it.len }
                        if (hit >= 0) {
                            s.notes.removeAt(hit)
                        } else if (key >= l) {
                            s.notes.add(N(t, step * s.lenSteps, key))
                        }
                        s.push()
                    }
                }
        ) {
            for (r in 0 until rows) {
                val key = hi - r
                val isBlack = (key % 12) in blackKeys
                drawRect(
                    if (isBlack) Color(0xFF1B1B22) else Color(0xFF22232B),
                    Offset(0f, r * rowPx),
                    Size(size.width, rowPx)
                )
                drawLine(
                    if (key % 12 == 0) Color(0x66FFFFFF) else Color(0x14FFFFFF),
                    Offset(0f, r * rowPx),
                    Offset(size.width, r * rowPx),
                    1f
                )
            }
            for (b in 0..totalBeats) {
                drawLine(
                    if (b % 4 == 0) Color(0x55FFFFFF) else Color(0x22FFFFFF),
                    Offset(b * beatPx, 0f),
                    Offset(b * beatPx, size.height),
                    if (b % 4 == 0) 2f else 1f
                )
            }
            for (n in s.notes) {
                val x = n.tick.toFloat() / s.ppq * beatPx
                val w = max(n.len.toFloat() / s.ppq * beatPx - 1f, 4f)
                val y = (hi - n.key) * rowPx
                drawRect(Color(0xFF4FC3F7), Offset(x, y + 1f), Size(w, rowPx - 2f))
            }
            val px = (s.position / s.ppq * beatPx).toFloat()
            drawLine(Color(0xFFFF7043), Offset(px, 0f), Offset(px, size.height), 3f)
        }
    }
}

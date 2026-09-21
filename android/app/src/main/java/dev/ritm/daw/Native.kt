package dev.ritm.daw

object Native {
    init {
        System.loadLibrary("ritmjni")
    }

    external fun start(): Boolean
    external fun shutdown()
    external fun setNotes(notes: IntArray, bpm: Double, ppq: Int, loopTicks: Double)
    external fun setBpm(bpm: Double)
    external fun play()
    external fun pause()
    external fun stopAndRewind()
    external fun isPlaying(): Boolean
    external fun position(): Double
    external fun importProject(path: String): String
    external fun importedNotes(): IntArray
    external fun importedBpm(): Double
    external fun importedPpq(): Int
}

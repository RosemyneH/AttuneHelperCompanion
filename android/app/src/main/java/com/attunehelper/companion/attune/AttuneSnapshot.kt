package com.attunehelper.companion.attune

data class AttuneSnapshot(
    val date: String,
    val account: Int,
    val warforged: Int,
    val lightforged: Int,
    val titanforged: Int,
)

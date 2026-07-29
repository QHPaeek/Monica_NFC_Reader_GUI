#include "mainwindow.h"

#include <QApplication>
#include <QDateTime>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QThread>

/* ====================================================================
 *  AIC 卡号解密 (S_BOX_INV) — 来自 STM32 固件 mode_sega_serial.c
 *  N_TABLES=8, ITER_ADD=0x05
 * ==================================================================== */
const uint8_t S_BOX_INV[9][256] = {
{0x24,0x3C,0xBA,0x36,0xE3,0x85,0xA4,0xD0,0x93,0x43,0x73,0xB9,0x70,0x6E,0xC9,0xF1,0x10,0x0E,0x9B,0x2C,0x97,0xE7,0x0B,0x63,0x6C,0x29,0x20,0xFE,0x86,0xF3,0xE1,0xF5,0xF6,0x9F,0xB6,0x16,0x04,0x7B,0x8F,0xAB,0xB1,0x39,0x2A,0x1B,0xEB,0x5C,0xA8,0xAC,0x38,0x11,0x12,0x5F,0x89,0x3E,0x7D,0xCA,0xEC,0x53,0xDB,0x6D,0x1E,0xD2,0x81,0x78,0x96,0x46,0xFF,0xF9,0x54,0x1C,0x28,0x7A,0x4F,0xD3,0xC0,0xDC,0xC1,0x6A,0xF2,0xBC,0xCB,0x57,0xFD,0x4A,0xE4,0xF0,0xB2,0xC7,0x95,0x40,0x62,0x52,0x41,0xE2,0xAD,0x49,0xA6,0xB5,0x1F,0x02,0xC8,0xDA,0x92,0xE5,0xB0,0xC5,0x64,0x76,0x48,0x21,0xDE,0x0F,0x45,0x58,0x4C,0xDF,0xA7,0x84,0xCF,0xD5,0x15,0x4E,0x27,0x80,0x6B,0x7F,0xFC,0x44,0x71,0x47,0x22,0xDD,0x30,0x0C,0xA1,0x3B,0xE0,0x37,0xA0,0x35,0x23,0x90,0x32,0x74,0xBF,0x8D,0xC2,0xEA,0xD6,0x50,0xBB,0xD9,0xC4,0x83,0xB4,0x31,0x68,0x55,0x8B,0x5D,0xF4,0x72,0x18,0xB7,0xEF,0xF7,0x98,0x2D,0x01,0x03,0x61,0xCC,0x0A,0x8E,0x13,0x00,0xE8,0x14,0xAF,0x09,0x51,0x75,0xA5,0x2F,0x1D,0x0D,0x65,0x8A,0xCD,0x66,0x07,0x3D,0x05,0xD8,0x4B,0xE9,0x9D,0x99,0x7C,0x91,0xD1,0xB8,0x19,0xC3,0x2B,0x42,0x69,0x88,0xC6,0x79,0x17,0x3A,0x4D,0x5B,0xA2,0x5A,0xFB,0x25,0x3F,0xBD,0xF8,0xED,0xCE,0xE6,0x87,0xA3,0x26,0xA9,0x2E,0xBE,0x94,0x08,0x7E,0x67,0x60,0x8C,0x9C,0x5E,0x6F,0xB3,0x9E,0x06,0xFA,0x82,0xAE,0xEE,0x59,0x77,0xD7,0x1A,0x9A,0x34,0xD4,0x56,0xAA,0x33,},
{0xF9,0x81,0x7C,0x00,0xB9,0x30,0x37,0xD5,0x90,0x51,0x6E,0xF0,0xB2,0x06,0xFB,0xCD,0x39,0x14,0x5F,0xF8,0x1B,0xC7,0x4A,0x82,0x70,0x8C,0x92,0x1D,0xEA,0xC3,0x4E,0xC8,0x12,0xE6,0xB6,0x10,0xD3,0x2F,0x95,0x84,0x25,0x42,0xA5,0x72,0xE5,0x8F,0x55,0xEF,0x86,0xA2,0x53,0xAE,0xED,0x26,0x20,0x47,0xDE,0x78,0x68,0x28,0xE4,0x45,0xCC,0x35,0xBC,0x3E,0xBB,0x8E,0xC0,0xBE,0x34,0xAF,0xA6,0x09,0x64,0x01,0x7B,0x44,0xF5,0xF3,0xB1,0x3F,0xA4,0xB3,0x32,0x80,0xC9,0x4F,0x6F,0x40,0xBF,0x21,0x4C,0x74,0xE1,0x11,0x5E,0xEB,0x3D,0x71,0x2E,0x9D,0x62,0x75,0xD4,0x16,0x48,0x77,0x13,0x67,0xAD,0x6B,0x5D,0x07,0x87,0xF7,0xA8,0x9A,0x59,0x76,0x8B,0x9B,0x1E,0xD8,0xEE,0xAA,0xE9,0x99,0x2D,0xC5,0x97,0xF1,0xA7,0x83,0xFC,0xCA,0xDC,0xBA,0xB8,0x4B,0xE8,0x89,0x17,0x05,0x0B,0xA0,0x65,0x23,0xD1,0xCE,0x36,0x03,0xD7,0xE0,0xF2,0x91,0xDF,0x0E,0x9C,0x2C,0x0D,0x66,0xD6,0x73,0x58,0x5C,0xB4,0x0A,0x4D,0xC2,0x3B,0xD2,0xB7,0xE2,0xAC,0x33,0xDB,0x60,0x27,0xBD,0x56,0x43,0x24,0x04,0x02,0x8D,0x7D,0x7F,0xF6,0xB5,0xF4,0xFD,0xDD,0x5B,0xEC,0x79,0xC4,0x22,0x1F,0xA1,0x88,0x54,0xE7,0x19,0x98,0x94,0x7E,0x31,0xA3,0x29,0xE3,0x5A,0xCB,0x6A,0xB0,0xCF,0x6D,0x93,0x6C,0x7A,0x08,0xA9,0x3C,0x1C,0xD0,0x63,0x50,0xC6,0x85,0x38,0xC1,0x41,0x49,0xAB,0x61,0x52,0x2A,0x9F,0xD9,0x18,0x1A,0x57,0x46,0x2B,0x69,0xDA,0xFA,0xFF,0x0F,0x15,0x96,0x3A,0x8A,0xFE,0x9E,0x0C,},
{0x11,0xA2,0x9A,0xC3,0x94,0xA0,0x51,0x01,0x5B,0xF7,0xD1,0x30,0xEE,0x1F,0x06,0xD5,0x77,0x67,0xD3,0xC1,0x6E,0xE7,0x6A,0x1A,0xA4,0x8E,0x9F,0x65,0x66,0xD6,0x0C,0x2D,0xC6,0xE4,0x69,0xB7,0x56,0x5A,0x57,0x60,0xFC,0x79,0x1E,0xE1,0x16,0x52,0xF0,0x07,0xD0,0xCD,0xCA,0x78,0xC9,0x8B,0xB3,0x88,0x27,0xF6,0xE0,0xC0,0x84,0xCF,0x2F,0xA3,0x04,0x00,0x80,0x40,0xA7,0xFA,0x75,0x9D,0x4B,0x89,0x46,0x22,0x28,0x37,0x34,0x13,0xFF,0x20,0xB4,0xDA,0x08,0x26,0x6C,0x8F,0xF2,0x5D,0x7B,0x73,0x5C,0x4C,0x90,0x71,0x41,0x8A,0x9E,0xBB,0x12,0xE8,0x55,0x6D,0x2A,0x25,0x4A,0xAF,0xBD,0x95,0x19,0xA5,0x31,0xBA,0xAD,0xD9,0xC5,0xA6,0x6B,0x83,0xC4,0xB6,0xA9,0xCC,0xF9,0xA1,0xB1,0x7A,0xDC,0xC7,0xDB,0x2E,0x7E,0x7F,0x8D,0x4E,0x62,0x0F,0x03,0x39,0xFD,0xB8,0xD4,0x18,0xC2,0xEC,0x61,0x02,0x53,0x1C,0x3B,0x7D,0xBC,0x1D,0x59,0xF5,0x8C,0x98,0xF1,0x6F,0x93,0x85,0xF8,0x2C,0x74,0xD8,0x5E,0x4D,0x97,0xAB,0x87,0x82,0x0A,0x21,0x42,0x5F,0x0D,0x58,0x33,0x44,0x3A,0xDD,0xE9,0xFB,0x50,0x47,0xC8,0xD7,0x81,0x9B,0xE5,0x1B,0x17,0x54,0x86,0x2B,0xEF,0xF4,0x29,0x32,0x0E,0x7C,0x23,0x15,0xEB,0xE6,0x3C,0x35,0xE2,0x96,0x68,0x3F,0x0B,0x43,0xCE,0xDE,0x9C,0xBE,0x4F,0x45,0x72,0x76,0xB5,0x10,0xE3,0xDF,0x92,0xD2,0xA8,0x48,0x91,0xB9,0xAC,0xBF,0x49,0xB2,0x99,0x64,0x70,0xEA,0xF3,0x3E,0x63,0x14,0xAE,0xED,0xAA,0x24,0xFE,0x3D,0xCB,0xB0,0x09,0x38,0x36,0x05,},
{0xC4,0xF0,0x28,0x49,0x55,0x4A,0xF5,0xFD,0x75,0xAF,0x20,0x69,0xC8,0x43,0x86,0x6B,0xC9,0xA8,0xC6,0x54,0x4C,0xDD,0x02,0x5B,0xE8,0x9B,0x59,0x77,0x34,0xD7,0xC0,0x51,0x5F,0xF3,0x7E,0xD4,0xF1,0x90,0x81,0xCE,0x19,0x8A,0x78,0x33,0xCD,0x97,0x8C,0xD6,0x6A,0x4B,0x8F,0xA4,0x80,0xDC,0x1A,0x17,0x14,0xC5,0x07,0x87,0x66,0xAD,0x9D,0x85,0xB2,0xF8,0x8D,0x98,0xDF,0x3E,0x1F,0x3F,0x64,0x58,0x40,0xAC,0x6C,0x5C,0x08,0x5D,0x53,0xBA,0xFF,0xD2,0xA9,0x2C,0x25,0xAB,0xE4,0x32,0x38,0x9A,0xF9,0xCB,0xC2,0x91,0x67,0xD0,0xE3,0x22,0x29,0x2D,0x92,0x48,0xB4,0x0E,0x99,0x05,0xA3,0x6F,0x0A,0x15,0xEF,0xD5,0x10,0x4F,0xD8,0xF6,0x00,0xE6,0x46,0x2B,0x31,0x57,0x83,0x94,0xAE,0x03,0xEB,0x8E,0x13,0x24,0xA1,0x1E,0x5E,0xD1,0x26,0xD9,0xA7,0xCA,0x36,0x6E,0x71,0x37,0xEE,0xB1,0xFA,0x7C,0x76,0x06,0xB8,0x23,0xBE,0x21,0xBC,0x9E,0xC1,0xDA,0x7A,0x3B,0x62,0x27,0x7B,0xB0,0xE5,0x63,0x18,0x82,0x7F,0x0F,0xED,0xA0,0x2A,0x9C,0xBF,0x11,0xBD,0xE0,0x88,0x0D,0x3A,0x79,0x52,0x56,0xF7,0xB6,0xD3,0x09,0x16,0x1B,0x70,0xB3,0x42,0x60,0x2F,0x1C,0xA2,0x9F,0x72,0x12,0x45,0x47,0xBB,0xE1,0x0B,0x01,0x8B,0x1D,0xDE,0xEC,0x0C,0x4E,0xE9,0xCF,0xCC,0x95,0x74,0xF4,0xA5,0x93,0xC7,0xDB,0x4D,0xFB,0x35,0x65,0xFE,0xE7,0x39,0xB5,0xEA,0x96,0xC3,0x04,0x41,0x44,0x84,0xFC,0x6D,0x30,0xE2,0xF2,0x68,0xB7,0x89,0x7D,0x73,0x3D,0xB9,0x5A,0x50,0xA6,0x3C,0x61,0xAA,0x2E,},
{0x1A,0x59,0x9C,0xAD,0xC8,0xE4,0x11,0x54,0xED,0x37,0x0F,0x3A,0xE6,0x5F,0x3C,0x4B,0xB8,0x15,0x89,0xB1,0xE8,0xDA,0x69,0x77,0x91,0x56,0x8B,0xDB,0x06,0x24,0xCF,0x18,0xF8,0xB0,0x87,0xDF,0x8C,0x35,0xCB,0x86,0x53,0x9D,0xA4,0x66,0x4A,0x7A,0x71,0x0A,0x48,0x38,0xFF,0xDC,0x83,0x20,0xCE,0x98,0x32,0xF6,0xD7,0xAF,0x70,0x5E,0x73,0x8A,0x14,0x72,0x1E,0xC1,0x29,0x79,0x07,0x08,0xE9,0x43,0x46,0xD0,0x2F,0xDE,0x2A,0x4F,0x3D,0x2C,0x50,0xB3,0x75,0xFC,0x0B,0x64,0xAE,0x31,0x7B,0x61,0xA5,0x30,0xA0,0x93,0xD2,0xBE,0xC2,0x55,0xBA,0x6C,0xF3,0x62,0x68,0xFD,0xAC,0x3B,0x95,0x49,0x1F,0x6B,0xB4,0x85,0xF7,0xA6,0x03,0xEC,0x6E,0x9A,0x81,0x09,0x0C,0x6A,0xEE,0x9E,0x4E,0xF0,0xAB,0x2D,0x7E,0xA1,0xE1,0xBB,0xC9,0xBC,0x41,0xF2,0xFA,0x2E,0x1B,0xDD,0x27,0x34,0xD3,0x60,0x04,0xB5,0x01,0xD6,0x40,0xF9,0xD5,0x02,0x47,0xD9,0xD8,0xE0,0x8F,0x2B,0xFB,0xB7,0xC5,0xEB,0x57,0x16,0xE5,0x78,0x8D,0xF4,0x00,0xCD,0x82,0x39,0xC6,0x96,0xBD,0xBF,0xE3,0x36,0x45,0x0E,0x26,0x1D,0x63,0xE7,0x84,0x3E,0xB6,0x9B,0x22,0xA3,0xF5,0x8E,0x23,0x6D,0xC3,0x99,0x7D,0x90,0x97,0x10,0x25,0x80,0xD4,0x4C,0xE2,0x74,0xCC,0xB2,0x5C,0x33,0xC7,0xEA,0x05,0x12,0x3F,0x51,0xA8,0xFE,0xF1,0x1C,0x42,0xCA,0xD1,0x76,0x28,0x6F,0x92,0xA7,0x67,0xA9,0x19,0xA2,0x44,0x5B,0xAA,0xEF,0x58,0x7F,0x21,0xC0,0x88,0x65,0xB9,0x5D,0x4D,0x0D,0x5A,0xC4,0x13,0x52,0x7C,0x9F,0x94,0x17,},
{0xC7,0x2B,0x82,0x61,0x5B,0xD0,0x96,0x84,0xD3,0x4A,0x70,0xA1,0x9B,0x59,0x33,0x9F,0xC0,0x20,0x14,0x53,0x29,0x17,0xC5,0x0B,0xC9,0x7B,0x97,0x02,0x0D,0x3A,0x1E,0x7C,0x3F,0x6B,0x52,0xE8,0x75,0x3D,0xF6,0xE4,0x0C,0x8A,0x4F,0xC3,0x5F,0x26,0x65,0x73,0x31,0x23,0x28,0x48,0x74,0xAA,0xA7,0x36,0x09,0xB1,0xE2,0x91,0x04,0x51,0x22,0xFC,0x08,0xA6,0x05,0xA4,0xF1,0x12,0x1C,0x19,0xEB,0x40,0x37,0xC2,0xA0,0x41,0x1D,0xD4,0xDC,0x07,0x43,0x8F,0x47,0xAF,0xD1,0x2E,0x98,0xAB,0x01,0xBA,0xF0,0x66,0x68,0xAC,0xF9,0xE7,0x69,0xB6,0xCB,0x8D,0x78,0x87,0x15,0x03,0xD5,0xDF,0xA3,0x1A,0x9D,0x6A,0xEA,0x2F,0x94,0x4E,0x9E,0x42,0xD7,0xB8,0x38,0x92,0xD8,0xBB,0xDE,0xDD,0x9A,0xBC,0xB0,0x4C,0x79,0xF4,0x58,0x3E,0xE9,0x83,0x81,0xFF,0xE3,0x55,0xFD,0x5D,0xB2,0xEF,0x9C,0x6D,0x54,0x99,0x60,0xDA,0x3B,0xEC,0xFA,0x11,0xD6,0xC4,0x2A,0xED,0x4B,0xAE,0x13,0xBF,0xB9,0x06,0x8B,0xE0,0x1F,0x7F,0x5A,0xAD,0x90,0x39,0x0F,0xF3,0xBD,0x46,0x6C,0x2C,0xF2,0xF8,0xFE,0xD9,0xE6,0x72,0x0E,0x89,0xBE,0x5E,0xC6,0xA8,0xCF,0xF5,0x57,0x7E,0x8C,0xB7,0xE1,0x88,0x7A,0xC8,0x1B,0x18,0xDB,0x6F,0x35,0xD2,0x16,0x32,0x64,0x63,0xA5,0x8E,0xF7,0xB4,0x76,0x95,0x86,0x7D,0xE5,0xA9,0x5C,0x85,0x00,0xC1,0x93,0x4D,0xFB,0x30,0xA2,0xB5,0x25,0x34,0x10,0x77,0x3C,0x67,0x71,0x2D,0x44,0x45,0x56,0x0A,0x50,0xB3,0xCD,0x62,0x80,0xCE,0x21,0x49,0x24,0xCA,0xEE,0x27,0x6E,0xCC,},
{0xA5,0xB0,0x6D,0xB2,0x51,0x55,0x1F,0xBF,0x3E,0x8D,0xDD,0x19,0x92,0xEA,0x1B,0xBD,0x32,0x65,0x29,0xA4,0x89,0x67,0xB1,0x90,0x68,0x00,0xDA,0x0D,0xA6,0x59,0x54,0xF2,0x10,0x14,0x2F,0x45,0xE0,0x3B,0x23,0xFA,0xD7,0x50,0xAC,0x93,0xE6,0x43,0x01,0x0A,0x5C,0x78,0x70,0x98,0x46,0xA9,0x7B,0xF3,0x95,0x07,0x2A,0xD3,0x74,0xB3,0x3F,0xA3,0x60,0x82,0x39,0x4E,0x34,0x48,0xC0,0x1C,0xF6,0xC2,0x91,0x64,0x4D,0x3C,0xF8,0x9D,0x35,0x9A,0x94,0xE3,0x7A,0xF0,0xF9,0x6E,0xB9,0x12,0xB8,0x04,0x2D,0x02,0x28,0x13,0x85,0x72,0x80,0x87,0xDB,0x2B,0xF1,0x4F,0x26,0xA2,0xE1,0x49,0x7E,0x9C,0xCC,0xA7,0xB6,0xA0,0xD0,0x9B,0x36,0x77,0xAD,0x8B,0x6B,0x4A,0x03,0x1D,0x05,0x8A,0x06,0x4B,0xAF,0xE5,0x31,0xB5,0xD4,0xC6,0x0C,0x66,0xBA,0x83,0xFD,0x09,0x0F,0xAE,0x71,0xB7,0x6F,0xDC,0x41,0xE8,0x17,0x8E,0x40,0x7F,0x62,0x30,0xFF,0xA8,0x84,0x25,0xFB,0x16,0xCE,0x37,0x44,0xAB,0x99,0x1E,0xEB,0x18,0x3A,0x47,0xF7,0x5F,0x81,0xCF,0xED,0x58,0x2C,0x6C,0xFE,0x9E,0x57,0x53,0x97,0x20,0xCA,0x79,0x1A,0x5A,0x88,0xF5,0x69,0x9F,0xE7,0xD9,0x0E,0xBE,0x42,0xDF,0x56,0xE4,0x4C,0x22,0xAA,0x73,0x0B,0x15,0xC5,0xEE,0xFC,0xC7,0xD6,0xCB,0xCD,0x8C,0xE2,0x76,0x21,0xE9,0xD1,0xEC,0xC8,0x7D,0xD8,0x8F,0x61,0x7C,0x2E,0xBC,0xDE,0xB4,0x75,0xD2,0xC4,0x63,0x3D,0xA1,0x5E,0x5D,0x6A,0x08,0x24,0xC9,0x27,0xBB,0xEF,0x33,0x86,0x5B,0xD5,0x38,0x52,0x11,0xF4,0xC3,0x96,0xC1,},
{0x34,0x5A,0xDB,0x2C,0x59,0x57,0x12,0x2B,0x30,0xC2,0xA0,0x92,0xBF,0xED,0xBC,0x45,0xDE,0x27,0x9B,0x96,0xD3,0xE6,0xC5,0xEB,0xD8,0x24,0x4B,0xA4,0x21,0xCC,0xA8,0xD6,0xCE,0x3C,0xBA,0xB1,0x09,0xE0,0xD7,0x32,0x66,0x4A,0x83,0x1D,0x19,0xCA,0x89,0x67,0x0F,0x42,0x07,0x71,0xE9,0xBB,0x44,0x5E,0x85,0x17,0xC4,0xAE,0x9C,0x3F,0x6B,0x78,0xE7,0x6D,0x02,0xA7,0xFE,0x33,0xE3,0xD9,0x0A,0x7D,0xEA,0xF1,0x18,0x87,0x7B,0x62,0x03,0x79,0x52,0x23,0x00,0x3E,0x25,0xB9,0xDD,0x16,0x68,0x3B,0x1F,0x90,0xA6,0x2A,0xC6,0x29,0x91,0x8A,0x9D,0xEF,0x1E,0x84,0x76,0xEE,0x4F,0x39,0xFB,0x11,0x1B,0x0B,0x93,0xAD,0x49,0xB7,0x05,0xE1,0x4D,0xCB,0xE8,0x38,0xD0,0x55,0xF2,0xF7,0x0D,0x8B,0x65,0xCD,0xFA,0xD4,0x9E,0xC9,0x81,0x4E,0x14,0xC8,0x26,0xB6,0xF9,0xAA,0xF5,0x80,0xF8,0x6A,0x98,0xAB,0x58,0xF3,0xB8,0x8E,0xB5,0x97,0x43,0x72,0xA2,0xDA,0x64,0xC1,0x40,0x5C,0x13,0xB0,0xE5,0xBD,0x08,0x9F,0x1C,0x7E,0x8F,0x06,0xBE,0x6E,0x50,0x1A,0x2F,0x37,0xA1,0xFC,0x10,0x2E,0x70,0x53,0x04,0x20,0x63,0x48,0xE2,0xFD,0x6F,0x7A,0x75,0x61,0xB3,0x35,0x3A,0xCF,0x5B,0x88,0x82,0x51,0xD2,0x47,0xA5,0xA9,0x74,0x6C,0x31,0x94,0x7C,0x9A,0xC0,0xEC,0x15,0x0E,0x28,0x01,0x2D,0xC3,0xF0,0xB4,0xE4,0x8D,0xDC,0xAC,0x3D,0x5F,0x86,0xC7,0x95,0x22,0xF4,0xB2,0xA3,0x54,0x46,0xD1,0x77,0x8C,0x0C,0xFF,0xAF,0x56,0x5D,0x36,0x69,0x7F,0x99,0x4C,0xD5,0x60,0x73,0xDF,0x41,0xF6,},
{0x04,0xDA,0x79,0x63,0x1E,0xBD,0xEA,0xE3,0x0B,0x65,0x25,0x01,0xCD,0xA9,0x92,0xED,0x18,0x75,0x57,0x30,0x89,0x03,0x09,0x6A,0xDC,0xC7,0x98,0xA4,0x50,0x91,0x1F,0xB1,0x0C,0x77,0x85,0x66,0xCA,0x12,0x1C,0x67,0xC2,0xE0,0x17,0x83,0x59,0x9D,0xD5,0x22,0x82,0x56,0xA8,0x9F,0xC6,0x60,0x48,0xB3,0x11,0xFC,0xA3,0x28,0xFB,0x06,0xDD,0x5D,0xBA,0x29,0x7A,0x4F,0xE8,0xFE,0xE9,0x10,0xCB,0xF3,0x93,0x7B,0x6C,0x69,0x54,0xE7,0x44,0xA2,0x84,0x1D,0x8D,0xCE,0xFF,0xFA,0x1A,0x87,0x90,0x74,0xA1,0xF8,0x14,0xAA,0xBC,0xC0,0xCF,0x31,0xB4,0xD9,0xBF,0xD8,0x5E,0x26,0x2D,0xD0,0xE4,0x3F,0x19,0xD6,0x8C,0x2F,0xAB,0x39,0x58,0x72,0x6E,0xDF,0x3B,0xE6,0x3C,0xB6,0x62,0x88,0xDE,0x40,0xB2,0x8F,0x9B,0x7C,0x95,0x43,0xD1,0x9E,0xAC,0x9C,0xD4,0x23,0xE1,0x0E,0x4B,0x53,0x33,0x46,0x20,0x13,0x34,0x1B,0x97,0xF7,0xF1,0xC3,0x61,0x4A,0x6F,0x5A,0x21,0x7F,0x70,0x2E,0x55,0x41,0x05,0xC5,0xD7,0x76,0xE5,0x27,0x15,0xEC,0x42,0x5C,0x4D,0x78,0x35,0x8B,0xEF,0xD2,0xEE,0xB5,0xBE,0xAE,0x02,0x3A,0xD3,0x5F,0xC4,0x24,0xF0,0xF9,0x51,0x4E,0xEB,0x00,0x0F,0xFD,0xAF,0x3E,0xC1,0x9A,0x52,0x86,0x81,0x80,0x7E,0xF6,0x2B,0xCC,0xB9,0x7D,0x68,0xF2,0xAD,0x99,0xA7,0x07,0x2C,0x73,0x38,0xB0,0x6B,0xB7,0x8E,0x71,0xA0,0xF4,0x3D,0xA6,0x0D,0x37,0xDB,0x0A,0x47,0x36,0x16,0x96,0x6D,0x32,0x2A,0x5B,0xE2,0x45,0x94,0xF5,0xA5,0x4C,0xC8,0x8A,0x49,0x64,0xBB,0x08,0xC9,0xB8,},
};

static constexpr uint8_t AIC_N_TABLES = 8;
static constexpr uint8_t AIC_ITER_ADD = 0x05;

static void decryptAicAccessCode(uint8_t *spad)
{
    for (int i = 0; i < 16; ++i)
        spad[i] = S_BOX_INV[AIC_N_TABLES][spad[i]];

    uint8_t count = (spad[15] >> 4) + 7;
    uint8_t table = spad[15] + AIC_ITER_ADD * count;
    for (uint8_t iter = 0; iter < count; ++iter) {
        table -= AIC_ITER_ADD;
        uint8_t prior = spad[14];
        for (int i = 0; i < 15; ++i) {
            uint8_t temp = spad[i];
            spad[i] = (spad[i] >> 5) | ((prior & 0x1F) << 3);
            prior = temp;
        }
        for (int i = 0; i < 15; ++i)
            spad[i] = S_BOX_INV[table % AIC_N_TABLES][spad[i]];
    }
}

static QString formatCardNumber(const QString &hex)
{
    QString fmt;
    for (int i = 0; i < hex.length(); ++i) {
        if (i > 0 && i % 4 == 0)
            fmt.append(QLatin1Char(' '));
        fmt.append(hex.at(i));
    }
    return fmt;
}

/* 检查 HEX 字符串是否为纯十进制卡号 (不含 A-F) */
static bool isValidDecimalCardNum(const QString &hex)
{
    for (const QChar &c : hex) {
        if (c >= QLatin1Char('A') && c <= QLatin1Char('F'))
            return false;
    }
    return true;
}

/* UID → 虚拟卡号 (hex_to_accesscode, 来自 STM32 固件)
 * 取 8 字节 UID, 转换成 10 字节 access code, 20 位 HEX 显示.
 */
static QString uidToAccessCode(const uint8_t *uid8)
{
    uint8_t ac[10];
    ac[0] = 0x00;
    ac[1] = 0x00;
    memcpy(ac + 2, uid8, 8);
    for (int i = 0; i < 8; ++i) {
        uint8_t hi = (uid8[i] >> 4);
        uint8_t lo = (uid8[i] & 0x0F);
        if (hi >= 10) hi -= 7;
        if (lo >= 10) lo -= 7;
        ac[i + 2] = (hi << 4) | lo;
    }
    QString hex;
    for (int i = 0; i < 10; ++i)
        hex.append(QStringLiteral("%1").arg(ac[i], 2, 16, QLatin1Char('0')));
    return formatCardNumber(hex.toUpper());
}

// ====================================================================
//  构造 & 析构
// ====================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_hid(new HIDWorker(nullptr))
    , m_connected(false)
    , m_langEn(false)
    , m_inDfuMode(false)
    , m_netManager(new QNetworkAccessManager(this))
    , m_dlProgress(nullptr)
    , m_dfuDownloadBtn(nullptr)
{
    memset(&m_flashData, 0, sizeof(FlashData));

    setupUI();
    applyStyleSheet();
    applyLanguage();      // 初始设置中文标签

    /* ---- HIDWorker 信号 ---- */
    connect(m_hid, &HIDWorker::deviceConnected,
            this, &MainWindow::onDeviceConnected);
    connect(m_hid, &HIDWorker::deviceDisconnected,
            this, &MainWindow::onDeviceDisconnected);
    connect(m_hid, &HIDWorker::deviceError,
            this, &MainWindow::onDeviceError);
    connect(m_hid, &HIDWorker::cardDetected,
            this, &MainWindow::onCardDetected);
    connect(m_hid, &HIDWorker::flashDataRead,
            this, &MainWindow::onFlashDataRead);
    connect(m_hid, &HIDWorker::flashDataWritten,
            this, &MainWindow::onFlashDataWritten);
    connect(m_hid, &HIDWorker::rawDataSent,
            this, &MainWindow::onRawDataSent);
    connect(m_hid, &HIDWorker::rawDataReceived,
            this, &MainWindow::onRawDataReceived);

    setControlsEnabled(false);
    updateConnectionUI(false);
}

MainWindow::~MainWindow()
{
    m_hid->stop();
    delete m_hid;
    m_hid = nullptr;
}

/* ---- 中/英文切换辅助 ---- */
QString MainWindow::trC(const QString &zh, const QString &en) const
{
    return m_langEn ? en : zh;
}

// ====================================================================
//  UI 构建
// ====================================================================

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("NFC Card Reader - Configuration Tool"));
    resize(1100, 750);
    setMinimumSize(900, 600);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    mainLayout->addWidget(buildHeaderPanel());

    QSplitter *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(buildCardMonitorPanel());
    m_stackedRight = new QStackedWidget(central);
    m_stackedRight->addWidget(buildSettingsPanel());
    m_stackedRight->addWidget(buildDfuPage());
    m_stackedRight->setCurrentIndex(0);
    splitter->addWidget(m_stackedRight);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({420, 680});
    mainLayout->addWidget(splitter, 1);

    m_lblStatusMsg = new QLabel(QStringLiteral("Ready - Click Connect to start"));
    m_lblStatusMsg->setObjectName(QStringLiteral("statusMsg"));
    statusBar()->addWidget(m_lblStatusMsg, 1);
    statusBar()->setObjectName(QStringLiteral("appStatusBar"));
}

// ====================================================================
//  Header
// ====================================================================

QWidget* MainWindow::buildHeaderPanel()
{
    QFrame *header = new QFrame(this);
    header->setObjectName(QStringLiteral("headerPanel"));
    header->setFixedHeight(56);

    QHBoxLayout *hbox = new QHBoxLayout(header);
    hbox->setContentsMargins(20, 0, 20, 0);

    QLabel *title = new QLabel(QStringLiteral("NFC Card Reader"));
    title->setObjectName(QStringLiteral("appTitle"));

    m_lblConnDot = new QLabel(this);
    m_lblConnDot->setObjectName(QStringLiteral("connDot"));
    m_lblConnDot->setFixedSize(10, 10);

    m_lblConnStatus = new QLabel(QStringLiteral("Disconnected"));
    m_lblConnStatus->setObjectName(QStringLiteral("connStatus"));

    m_lblFwVersion   = new QLabel(QStringLiteral("FW: --"));
    m_lblFwVersion->setObjectName(QStringLiteral("fwVersion"));

    m_lblReaderModel = new QLabel(this);
    m_lblReaderModel->setObjectName(QStringLiteral("readerModel"));
    m_lblReaderModel->hide();

    m_btnConnect    = new QPushButton(QStringLiteral("Connect"));
    m_btnDisconnect = new QPushButton(QStringLiteral("Disconnect"));
    m_btnConnect->setObjectName(QStringLiteral("btnConnect"));
    m_btnDisconnect->setObjectName(QStringLiteral("btnDisconnect"));
    m_btnDisconnect->hide();

    m_btnLang = new QPushButton(QStringLiteral("EN"));
    m_btnLang->setObjectName(QStringLiteral("btnLang"));
    m_btnLang->setFixedSize(40, 30);
    m_btnLang->setToolTip(QStringLiteral("Switch to English"));

    connect(m_btnConnect,    &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked,
            this, &MainWindow::onDisconnectClicked);
    connect(m_btnLang,       &QPushButton::clicked,
            this, &MainWindow::onToggleLanguage);

    m_btnDfu = new QPushButton(QStringLiteral("DFU"));
    m_btnDfu->setObjectName(QStringLiteral("btnDfu"));
    m_btnDfu->setToolTip(QString::fromUtf8("跳转到固件升级模式"));
    connect(m_btnDfu, &QPushButton::clicked,
            this, &MainWindow::onJumpToDfu);

    hbox->addWidget(title);
    hbox->addStretch();
    hbox->addWidget(m_lblReaderModel);
    hbox->addSpacing(12);
    hbox->addWidget(m_lblConnDot);
    hbox->addWidget(m_lblConnStatus);
    hbox->addSpacing(20);
    hbox->addWidget(m_lblFwVersion);
    hbox->addStretch();
    hbox->addWidget(m_btnLang);
    hbox->addSpacing(8);
    hbox->addWidget(m_btnDfu);
    hbox->addSpacing(8);
    hbox->addWidget(m_btnConnect);
    hbox->addWidget(m_btnDisconnect);

    return header;
}

// ====================================================================
//  左侧: 卡片监控
// ====================================================================

QWidget* MainWindow::buildCardMonitorPanel()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("cardMonitorPanel"));
    QVBoxLayout *vbox = new QVBoxLayout(panel);
    vbox->setContentsMargins(16, 16, 8, 16);
    vbox->setSpacing(16);

    /* 卡片 UID 显示 */
    m_cardDisplayFrame = new QFrame(panel);
    m_cardDisplayFrame->setObjectName(QStringLiteral("cardDisplay"));
    m_cardDisplayFrame->setMinimumHeight(180);

    QVBoxLayout *displayLayout = new QVBoxLayout(m_cardDisplayFrame);
    displayLayout->setAlignment(Qt::AlignCenter);
    displayLayout->setSpacing(6);

    QLabel *uidTitle = new QLabel(QStringLiteral("CARD NUMBER"));
    uidTitle->setObjectName(QStringLiteral("cardUidTitle"));

    m_lblCardUid = new QLabel(QStringLiteral("Waiting for card..."));
    m_lblCardUid->setObjectName(QStringLiteral("cardUidValue"));
    m_lblCardUid->setAlignment(Qt::AlignCenter);
    m_lblCardUid->setWordWrap(false);

    m_lblCardNumber = new QLabel(this);
    m_lblCardNumber->setObjectName(QStringLiteral("cardNumberLabel"));
    m_lblCardNumber->setAlignment(Qt::AlignCenter);
    m_lblCardNumber->setWordWrap(false);
    m_lblCardNumber->hide();

    m_lblCardType = new QLabel(this);
    m_lblCardType->setObjectName(QStringLiteral("cardTypeLabel"));
    m_lblCardType->setAlignment(Qt::AlignCenter);
    m_lblCardType->hide();

    displayLayout->addWidget(uidTitle);
    displayLayout->addWidget(m_lblCardNumber);
    displayLayout->addWidget(m_lblCardUid);
    displayLayout->addWidget(m_lblCardType);

    vbox->addWidget(m_cardDisplayFrame);

    /* 原始数据日志 */
    QLabel *logTitle = new QLabel(QStringLiteral("RAW DATA LOG"));
    logTitle->setObjectName(QStringLiteral("sectionTitle"));

    m_rawLog = new QTextEdit(panel);
    m_rawLog->setObjectName(QStringLiteral("rawLog"));
    m_rawLog->setReadOnly(true);
    m_rawLog->setLineWrapMode(QTextEdit::NoWrap);
    m_rawLog->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_rawLog->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_rawLog->document()->setMaximumBlockCount(500);

    vbox->addWidget(logTitle);
    vbox->addWidget(m_rawLog, 1);

    return panel;
}

// ====================================================================
//  右侧: 设置面板
// ====================================================================

QWidget* MainWindow::buildSettingsPanel()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("settingsPanel"));
    QVBoxLayout *vbox = new QVBoxLayout(panel);
    vbox->setContentsMargins(16, 16, 20, 16);
    vbox->setSpacing(16);

    QLabel *title = new QLabel(QStringLiteral("DEVICE SETTINGS"));
    title->setObjectName(QStringLiteral("sectionTitle"));

    m_tabSettings = new QTabWidget(panel);
    m_tabSettings->setObjectName(QStringLiteral("settingsTabs"));
    m_tabSettings->addTab(createSegaTab(),   QStringLiteral("SEGA"));
    m_tabSettings->addTab(createSpiceTab(),  QStringLiteral("SPICE"));
    m_tabSettings->addTab(createNamcoTab(),  QStringLiteral("NAMCO"));
    m_tabSettings->addTab(createSystemTab(), QStringLiteral("SYSTEM"));

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    m_btnRead  = new QPushButton(QStringLiteral("Read from Device"));
    m_btnWrite = new QPushButton(QStringLiteral("Write to Device"));
    m_btnReset = new QPushButton(QStringLiteral("Reset Defaults"));
    m_btnRead->setObjectName(QStringLiteral("btnPrimary"));
    m_btnWrite->setObjectName(QStringLiteral("btnPrimary"));
    m_btnReset->setObjectName(QStringLiteral("btnSecondary"));

    connect(m_btnRead,  &QPushButton::clicked,
            this, &MainWindow::onReadSettings);
    connect(m_btnWrite, &QPushButton::clicked,
            this, &MainWindow::onWriteSettings);
    connect(m_btnReset, &QPushButton::clicked,
            this, &MainWindow::onResetDefaults);

    btnRow->addWidget(m_btnRead);
    btnRow->addWidget(m_btnWrite);
    btnRow->addStretch();
    btnRow->addWidget(m_btnReset);

    vbox->addWidget(title);
    vbox->addWidget(m_tabSettings, 1);
    vbox->addLayout(btnRow);

    return panel;
}

/* ---- DFU 固件升级页面 ---- */
QWidget* MainWindow::buildDfuPage()
{
    QWidget *w = new QWidget(this);
    w->setObjectName(QStringLiteral("dfuPage"));
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(16);

    QLabel *title = new QLabel(QString::fromUtf8("DFU 固件升级"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    QLabel *desc  = new QLabel(QString::fromUtf8(
        "读卡器已进入 DFU 模式。选择固件文件并烧录到 STM32F072。\n"
        "支持格式: .bin / .hex / .dfu"));
    desc->setObjectName(QStringLiteral("tabDesc"));
    desc->setWordWrap(true);

    // 文件选择行
    QHBoxLayout *fileRow = new QHBoxLayout();
    m_dfuFileEdit = new QLineEdit(w);
    m_dfuFileEdit->setObjectName(QStringLiteral("dfuFileEdit"));
    m_dfuFileEdit->setPlaceholderText(
        QString::fromUtf8("选择固件文件 (.bin/.hex/.dfu) ..."));
    m_dfuFileEdit->setReadOnly(true);

    m_dfuBrowseBtn = new QPushButton(QString::fromUtf8("浏览..."), w);
    m_dfuBrowseBtn->setObjectName(QStringLiteral("btnSecondary"));
    connect(m_dfuBrowseBtn, &QPushButton::clicked,
            this, &MainWindow::onDfuFileBrowse);

    fileRow->addWidget(m_dfuFileEdit, 1);
    fileRow->addWidget(m_dfuBrowseBtn);

    // 目标信息
    QLabel *targetLabel = new QLabel(
        QStringLiteral("Target: STM32F072  @  0x08000000"), w);
    targetLabel->setObjectName(QStringLiteral("tabDesc"));

    // 工具检测 + 下载按钮
    QHBoxLayout *toolRow = new QHBoxLayout();
    m_dfuDownloadBtn = new QPushButton(QString::fromUtf8("未找到 dfu-util, 点击下载"), w);
    m_dfuDownloadBtn->setObjectName(QStringLiteral("btnSecondary"));
    m_dfuDownloadBtn->setVisible(false);
    connect(m_dfuDownloadBtn, &QPushButton::clicked,
            this, &MainWindow::onDfuCheckTool);

    // 烧录按钮
    m_dfuFlashBtn = new QPushButton(QString::fromUtf8("烧录固件"), w);
    m_dfuFlashBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_dfuFlashBtn->setEnabled(false);
    connect(m_dfuFlashBtn, &QPushButton::clicked,
            this, &MainWindow::onDfuFlash);

    // 日志输出
    m_dfuLog = new QPlainTextEdit(w);
    m_dfuLog->setObjectName(QStringLiteral("dfuLog"));
    m_dfuLog->setReadOnly(true);
    m_dfuLog->setMaximumBlockCount(200);
    m_dfuLog->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background:#1E1E2E; color:#CCCCCC; "
                       "font-family:Consolas,monospace; font-size:12px; }"));

    // QProcess
    m_dfuProcess = new QProcess(this);

    connect(m_netManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onDfuDownloadFinished);

    connect(m_netManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onDfuDownloadFinished);

    toolRow->addWidget(m_dfuDownloadBtn);
    toolRow->addStretch();

    vbox->addWidget(title);
    vbox->addWidget(desc);
    vbox->addLayout(fileRow);
    vbox->addWidget(targetLabel);
    vbox->addLayout(toolRow);
    vbox->addWidget(m_dfuFlashBtn);
    vbox->addWidget(m_dfuLog, 1);

    return w;
}

static QCheckBox* makeCheckBox(const QString &text, QWidget *parent)
{
    QCheckBox *cb = new QCheckBox(text, parent);
    cb->setObjectName(QStringLiteral("settingCheck"));
    return cb;
}

/* ---- SEGA ---- */
QWidget* MainWindow::createSegaTab()
{
    QWidget *w = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(14);

    QLabel *lbl = new QLabel(
        QStringLiteral("SEGA — Card type support settings for SEGA-compatible games."),
        w);
    lbl->setObjectName(QStringLiteral("tabDesc"));
    lbl->setWordWrap(true);
    vbox->addWidget(lbl);
    vbox->addSpacing(8);

    m_sega_tn32    = makeCheckBox(QString::fromUtf8(
        "启用 TN32 模拟。默认：模拟 15693 读卡器"), w);
    m_sega_aic     = makeCheckBox(QString::fromUtf8(
        "FeliCa AIC 卡号转换（转为真实序列号）"), w);
    m_sega_nesica  = makeCheckBox(QString::fromUtf8(
        "启用 NESiCA 支持（MIFARE Ultralight）"), w);
    m_sega_ea_pass = makeCheckBox(QString::fromUtf8(
        "启用 e-Amusement Pass 支持（ISO 15693）"), w);
    m_sega_tunion  = makeCheckBox(QString::fromUtf8(
        "启用交通联合支持（ISO 14443-A，APDU）"), w);
    m_sega_jubeat  = makeCheckBox(QString::fromUtf8(
        "启用乐动魔方支持（Jubeat China，MIFARE）"), w);

    vbox->addWidget(m_sega_tn32);
    vbox->addWidget(m_sega_aic);
    vbox->addWidget(m_sega_nesica);
    vbox->addWidget(m_sega_ea_pass);
    vbox->addWidget(m_sega_tunion);
    vbox->addWidget(m_sega_jubeat);
    vbox->addStretch();
    return w;
}

/* ---- SPICE ---- */
QWidget* MainWindow::createSpiceTab()
{
    QWidget *w = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(14);

    QLabel *lbl = new QLabel(
        QStringLiteral("SPICE — Card type support settings for SPICE-compatible games."),
        w);
    lbl->setObjectName(QStringLiteral("tabDesc"));
    lbl->setWordWrap(true);
    vbox->addWidget(lbl);
    vbox->addSpacing(8);

    m_spice_2p       = makeCheckBox(QString::fromUtf8("启用 2P 模式"), w);
    m_spice_aime     = makeCheckBox(QString::fromUtf8(
        "启用 Aime 支持（MIFARE Classic）"), w);
    m_spice_banapass = makeCheckBox(QString::fromUtf8(
        "启用 Banapass 支持（MIFARE Classic）"), w);
    m_spice_nesica   = makeCheckBox(QString::fromUtf8(
        "启用 NESiCA 支持（MIFARE Ultralight）"), w);
    m_spice_tunion   = makeCheckBox(QString::fromUtf8(
        "启用交通联合支持（ISO 14443-A，APDU）"), w);
    m_spice_jubeat   = makeCheckBox(QString::fromUtf8(
        "启用乐动魔方支持（Jubeat China，MIFARE）"), w);

    vbox->addWidget(m_spice_2p);
    vbox->addWidget(m_spice_aime);
    vbox->addWidget(m_spice_banapass);
    vbox->addWidget(m_spice_nesica);
    vbox->addWidget(m_spice_tunion);
    vbox->addWidget(m_spice_jubeat);
    vbox->addStretch();
    return w;
}

/* ---- NAMCO ---- */
QWidget* MainWindow::createNamcoTab()
{
    QWidget *w = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(14);

    QLabel *lbl = new QLabel(
        QStringLiteral("NAMCO — Card type support settings for NAMCO-compatible games."),
        w);
    lbl->setObjectName(QStringLiteral("tabDesc"));
    lbl->setWordWrap(true);
    vbox->addWidget(lbl);
    vbox->addSpacing(8);

    m_namco_todo     = makeCheckBox(QStringLiteral("（保留）"), w);
    m_namco_aime     = makeCheckBox(QString::fromUtf8(
        "启用 Aime 支持（MIFARE Classic）"), w);
    m_namco_banapass = makeCheckBox(QString::fromUtf8(
        "启用 Banapass 支持（MIFARE Classic）"), w);
    m_namco_nesica   = makeCheckBox(QString::fromUtf8(
        "启用 NESiCA 支持（MIFARE Ultralight）"), w);
    m_namco_tunion   = makeCheckBox(QString::fromUtf8(
        "启用交通联合支持（ISO 14443-B）"), w);
    m_namco_jubeat   = makeCheckBox(QString::fromUtf8(
        "启用乐动魔方支持（Jubeat China，MIFARE）"), w);

    vbox->addWidget(m_namco_todo);
    vbox->addWidget(m_namco_aime);
    vbox->addWidget(m_namco_banapass);
    vbox->addWidget(m_namco_nesica);
    vbox->addWidget(m_namco_tunion);
    vbox->addWidget(m_namco_jubeat);
    vbox->addStretch();
    return w;
}

/* ---- SYSTEM (去掉了 AIC 转换 — 已在 SEGA BIT1) ---- */
QWidget* MainWindow::createSystemTab()
{
    QWidget *w = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(18);

    QLabel *lbl = new QLabel(
        QStringLiteral("System — Hardware-level UART and LED settings."),
        w);
    lbl->setObjectName(QStringLiteral("tabDesc"));
    lbl->setWordWrap(true);
    vbox->addWidget(lbl);
    vbox->addSpacing(12);

    /* UART Baudrate */
    QGroupBox *grpUart = new QGroupBox(QString::fromUtf8("串口波特率"), w);
    grpUart->setObjectName(QStringLiteral("sysGroup"));
    QHBoxLayout *uartRow = new QHBoxLayout(grpUart);
    m_cmbUartBaud = new QComboBox(grpUart);
    m_cmbUartBaud->setObjectName(QStringLiteral("sysCombo"));
    m_cmbUartBaud->addItem(QStringLiteral("115200"), 0x00);
    m_cmbUartBaud->addItem(QStringLiteral("38400"),  0x01);
    m_cmbUartBaud->addItem(QStringLiteral("9600"),   0x02);
    m_cmbUartBaud->setMinimumWidth(200);
    uartRow->addWidget(m_cmbUartBaud);
    uartRow->addStretch();
    vbox->addWidget(grpUart);

    /* LED Brightness */
    QGroupBox *grpLed = new QGroupBox(QString::fromUtf8("LED 全局亮度"), w);
    grpLed->setObjectName(QStringLiteral("sysGroup"));
    QHBoxLayout *ledRow = new QHBoxLayout(grpLed);

    m_sldLedBright = new QSlider(Qt::Horizontal, grpLed);
    m_sldLedBright->setObjectName(QStringLiteral("ledSlider"));
    m_sldLedBright->setRange(0, 255);
    m_sldLedBright->setTickPosition(QSlider::TicksBelow);
    m_sldLedBright->setTickInterval(32);

    m_lblLedValue = new QLabel(QStringLiteral("0 / 255"), grpLed);
    m_lblLedValue->setObjectName(QStringLiteral("ledValue"));
    m_lblLedValue->setFixedWidth(70);
    m_lblLedValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    connect(m_sldLedBright, &QSlider::valueChanged, this, [this](int v) {
        m_lblLedValue->setText(QStringLiteral("%1 / 255").arg(v));
    });
    connect(m_sldLedBright, &QSlider::valueChanged,
            this, &MainWindow::onLedSliderChanged);

    QLabel *ledHint = new QLabel(
        QStringLiteral("0 = Off    |    128 = 50%    |    255 = Max"), grpLed);
    ledHint->setObjectName(QStringLiteral("sysHint"));

    ledRow->addWidget(m_sldLedBright, 1);
    ledRow->addSpacing(12);
    ledRow->addWidget(m_lblLedValue);
    vbox->addWidget(grpLed);
    vbox->addWidget(ledHint);
    vbox->addStretch();

    return w;
}

// ====================================================================
//  Bit ↔ Checkbox
// ====================================================================

void MainWindow::bindCheckboxesToBits(uint8_t byteVal,
                                      const QCheckBox *const *cbs, int count)
{
    for (int i = 0; i < count && i < 8; ++i) {
        if (cbs[i])
            const_cast<QCheckBox*>(cbs[i])->setChecked(byteVal & (1u << i));
    }
}

uint8_t MainWindow::collectBitsFromCheckboxes(
            const QCheckBox *const *cbs, int count) const
{
    uint8_t val = 0;
    for (int i = 0; i < count && i < 8; ++i) {
        if (cbs[i] && cbs[i]->isChecked())
            val |= (1u << i);
    }
    return val;
}

// ====================================================================
//  FlashData ↔ UI
// ====================================================================

void MainWindow::populateSettingsUI(const FlashData &data)
{
    const QCheckBox *sega[] = {
        m_sega_tn32, m_sega_aic, nullptr, m_sega_nesica,
        m_sega_ea_pass, m_sega_tunion, m_sega_jubeat
    };
    bindCheckboxesToBits(data.sega_setting, sega, 7);

    const QCheckBox *spice[] = {
        m_spice_2p, m_spice_aime, m_spice_banapass, m_spice_nesica,
        nullptr, m_spice_tunion, m_spice_jubeat
    };
    bindCheckboxesToBits(data.spice_setting, spice, 7);

    const QCheckBox *namco[] = {
        m_namco_todo, m_namco_aime, m_namco_banapass, m_namco_nesica,
        nullptr, m_namco_tunion, m_namco_jubeat
    };
    bindCheckboxesToBits(data.namco_setting, namco, 7);

    // UART (system_setting 即原来 system_setting_1)
    int baudIdx = m_cmbUartBaud->findData(data.system_setting);
    if (baudIdx >= 0)
        m_cmbUartBaud->setCurrentIndex(baudIdx);

    m_sldLedBright->blockSignals(true);
    m_sldLedBright->setValue(static_cast<int>(data.gobal_LED_setting));
    m_lblLedValue->setText(
        QStringLiteral("%1 / 255").arg(data.gobal_LED_setting));
    m_sldLedBright->blockSignals(false);
}

FlashData MainWindow::collectSettingsFromUI() const
{
    FlashData data;
    memset(&data, 0, sizeof(FlashData));

    const QCheckBox *sega[] = {
        m_sega_tn32, m_sega_aic, nullptr, m_sega_nesica,
        m_sega_ea_pass, m_sega_tunion, m_sega_jubeat
    };
    data.sega_setting = collectBitsFromCheckboxes(sega, 7);

    const QCheckBox *spice[] = {
        m_spice_2p, m_spice_aime, m_spice_banapass, m_spice_nesica,
        nullptr, m_spice_tunion, m_spice_jubeat
    };
    data.spice_setting = collectBitsFromCheckboxes(spice, 7);

    const QCheckBox *namco[] = {
        m_namco_todo, m_namco_aime, m_namco_banapass, m_namco_nesica,
        nullptr, m_namco_tunion, m_namco_jubeat
    };
    data.namco_setting = collectBitsFromCheckboxes(namco, 7);

    data.system_setting    = static_cast<uint8_t>(
        m_cmbUartBaud->currentData().toUInt());
    data.gobal_LED_setting = static_cast<uint8_t>(m_sldLedBright->value());

    return data;
}

void MainWindow::setControlsEnabled(bool enabled)
{
    m_tabSettings->setEnabled(enabled);
    m_btnRead->setEnabled(enabled);
    m_btnWrite->setEnabled(enabled);
    m_btnReset->setEnabled(enabled);
}

void MainWindow::updateConnectionUI(bool connected)
{
    if (connected) {
        m_lblConnDot->setProperty("status", "connected");
        m_lblConnStatus->setText(trC(QStringLiteral("已连接"),
                                     QStringLiteral("Connected")));
        m_btnConnect->hide();
        m_btnDisconnect->show();
    } else {
        m_lblConnDot->setProperty("status", "disconnected");
        m_lblConnStatus->setText(trC(QStringLiteral("未连接"),
                                     QStringLiteral("Disconnected")));
        m_btnConnect->show();
        m_btnDisconnect->hide();
    }
    m_lblConnDot->style()->unpolish(m_lblConnDot);
    m_lblConnDot->style()->polish(m_lblConnDot);
}

// ====================================================================
//  卡片 UID / 卡号 提取
// ====================================================================

QString MainWindow::cardTypeName(uint8_t cardType)
{
    switch (cardType) {
        case Card_Aime:          return QStringLiteral("Aime (MIFARE)");
        case Card_Bana_New:      return QStringLiteral("Banapass (New)");
        case Card_Bana_Old:      return QStringLiteral("Banapass (Old)");
        case Card_AIC:           return QStringLiteral("AIC (FeliCa)");
        case Card_EPass:         return QStringLiteral("e-Pass (ISO15693)");
        case Card_Nesica:        return QStringLiteral("NESiCA (Ultralight)");
        case Card_Tunion:        return QStringLiteral("T-Union (14443-A)");
        case Card_Jubeat:        return QStringLiteral("Jubeat (MIFARE)");
        case Card_Unknow_Mifare: return QStringLiteral("Unknown MIFARE");
        case Card_Blank_Mifare:  return QStringLiteral("Blank MIFARE");
        default:                 return QStringLiteral("Unknown (0x%1)")
                                        .arg(cardType, 2, 16, QLatin1Char('0'));
    }
}

QString MainWindow::extractUid(uint8_t cardType, const QByteArray &rawData)
{
    if (rawData.size() < 4) return QStringLiteral("(short)");

    switch (cardType) {
    case Card_Aime:
    case Card_Bana_New:
    case Card_Bana_Old:
    case Card_Jubeat:
    case Card_Tunion:
        // Card_RawData[0] 前 4 字节 = UID
        return QStringLiteral("%1 %2 %3 %4")
            .arg((uint8_t)rawData[0], 2, 16, QLatin1Char('0'))
            .arg((uint8_t)rawData[1], 2, 16, QLatin1Char('0'))
            .arg((uint8_t)rawData[2], 2, 16, QLatin1Char('0'))
            .arg((uint8_t)rawData[3], 2, 16, QLatin1Char('0'))
            .toUpper();

    case Card_AIC:
    case Card_EPass:
        // Card_RawData[0] 前 8 字节 = UID/IDm
        if (rawData.size() < 8) break;
        {
            QStringList parts;
            for (int i = 0; i < 8; ++i)
                parts.append(QStringLiteral("%1")
                    .arg((uint8_t)rawData[i], 2, 16, QLatin1Char('0')));
            return parts.join(QLatin1Char(' ')).toUpper();
        }

    case Card_Nesica:
        // Card_RawData[0] 前 7 字节 = UID
        if (rawData.size() < 7) break;
        {
            QStringList parts;
            for (int i = 0; i < 7; ++i)
                parts.append(QStringLiteral("%1")
                    .arg((uint8_t)rawData[i], 2, 16, QLatin1Char('0')));
            return parts.join(QLatin1Char(' ')).toUpper();
        }
    }

    // 默认: 显示前 8 字节
    QStringList parts;
    int n = qMin(rawData.size(), 8);
    for (int i = 0; i < n; ++i)
        parts.append(QStringLiteral("%1")
            .arg((uint8_t)rawData[i], 2, 16, QLatin1Char('0')));
    return parts.join(QLatin1Char(' ')).toUpper();
}

/* 从 Card_RawData 提取明文卡号 (Card Number)
 *
 * Aime/Bana_New:  Card_RawData[2] 偏移6, 10字节HEX卡号
 * Bana_Old:       加密卡号 (不显示)
 * AIC:            Card_RawData[1] 加密 (不显示)
 * EPass:          Card_RawData[1] 偏移0, 10字节HEX卡号
 * Nesica:         Card_RawData[1] 偏移0, 10字节HEX卡号
 * Tunion:         Card_RawData[1] 偏移0, 10字节HEX卡号
 * Jubeat:         无明文卡号 (用UID构造)
 */
QString MainWindow::extractCardNumber(uint8_t cardType,
                                      const QByteArray &rawData)
{
    int offset = 0;
    int len    = 0;

    switch (cardType) {
    case Card_Aime:
    case Card_Bana_New:
        // Card_RawData[2] (offset 32) 偏移6, 10字节
        if (rawData.size() < 32 + 6 + 10) return QString();
        offset = 32 + 6;
        len    = 10;
        break;
    case Card_Bana_Old:
        return QString();   // 加密卡号, 不显示
    case Card_AIC:
        // Card_RawData[1] (offset 16), 16 字节加密数据
        if (rawData.size() < 16 + 16) return QString();
        {
            uint8_t spad[16];
            memcpy(spad, rawData.constData() + 16, 16);
            decryptAicAccessCode(spad);
            // 去掉前导零
            int start = 0;
            while (start < 16 && spad[start] == 0x00)
                ++start;
            int validLen = 16 - start;
            if (validLen > 0) {
                QString hex;
                for (int i = start; i < 16; ++i)
                    hex.append(QStringLiteral("%1")
                        .arg(spad[i], 2, 16, QLatin1Char('0')));
                // 前导零必须 >= 6 字节 (12 hex 字符), 且卡号必须是纯十进制
                if (start >= 6 && isValidDecimalCardNum(hex))
                    return formatCardNumber(hex.toUpper());
            }
            // 不合格 → 用 UID 构造虚拟卡号
            if (rawData.size() >= 8)
                return uidToAccessCode(
                    reinterpret_cast<const uint8_t*>(rawData.constData()));
        }
        break;
    case Card_EPass:
    case Card_Nesica:
    case Card_Tunion:
        // Card_RawData[1] (offset 16) 偏移0, 10字节
        if (rawData.size() < 16 + 10) return QString();
        offset = 16;
        len    = 10;
        break;
    case Card_Jubeat:
    case Card_Unknow_Mifare:
    case Card_Blank_Mifare:
        return QString();
    default:
        return QString();
    }

    if (offset + len > rawData.size()) return QString();

    // 原始字节 → HEX 字符串, 每 4 字符加空格
    QString hex;
    for (int i = 0; i < len; ++i)
        hex.append(QStringLiteral("%1")
            .arg(static_cast<uint8_t>(rawData[offset + i]), 2, 16,
                 QLatin1Char('0')));
    return formatCardNumber(hex.toUpper());
}

void MainWindow::showCardInDisplay(const QString &uid, const QString &cardType,
                                   const QString &cardNumber)
{
    // UID: 小字灰色
    m_lblCardUid->setText(uid);
    m_lblCardUid->setProperty("hasCard", true);
    m_lblCardUid->style()->unpolish(m_lblCardUid);
    m_lblCardUid->style()->polish(m_lblCardUid);

    // 卡号: 大字金色, 主要显示 (无卡号时显示占位符)
    if (!cardNumber.isEmpty()) {
        m_lblCardNumber->setText(cardNumber);
        m_lblCardNumber->setProperty("placeholder", false);
    } else {
        m_lblCardNumber->setText(
            QStringLiteral("---- ---- ---- ---- ----"));
        m_lblCardNumber->setProperty("placeholder", true);
    }
    m_lblCardNumber->style()->unpolish(m_lblCardNumber);
    m_lblCardNumber->style()->polish(m_lblCardNumber);
    m_lblCardNumber->show();

    m_lblCardType->setText(cardType);
    m_lblCardType->show();
}

void MainWindow::clearCardDisplay()
{
    m_lblCardUid->setText(trC(
        QStringLiteral("等待刷卡..."),
        QStringLiteral("Waiting for card...")));
    m_lblCardUid->setProperty("hasCard", false);
    m_lblCardUid->style()->unpolish(m_lblCardUid);
    m_lblCardUid->style()->polish(m_lblCardUid);

    m_lblCardNumber->hide();
    m_lblCardType->hide();
}

// ====================================================================
//  按钮点击
// ====================================================================

void MainWindow::onConnectClicked()
{
    m_stackedRight->setCurrentIndex(0);  // 切换回设置页
    m_btnDfu->show();
    m_lblStatusMsg->setText(trC(
        QStringLiteral("正在搜索设备..."),
        QStringLiteral("Searching for device...")));
    m_btnConnect->setEnabled(false);
    m_hid->stop();
    m_hid->start();
}

void MainWindow::onDisconnectClicked()
{
    m_hid->stop();
}

void MainWindow::onReadSettings()
{
    m_lblStatusMsg->setText(trC(
        QStringLiteral("正在从设备读取设置..."),
        QStringLiteral("Reading settings from device...")));
    m_hid->requestFlashRead();
}

void MainWindow::onWriteSettings()
{
    FlashData data = collectSettingsFromUI();
    data.firmversion_1 = m_flashData.firmversion_1;
    data.firmversion_2 = m_flashData.firmversion_2;

    auto result = QMessageBox::question(
        this,
        trC(QStringLiteral("确认写入"),
            QStringLiteral("Confirm Write")),
        trC(QString::fromUtf8(
            "即将向设备写入配置。\n\n此操作会更新读卡器内部 Flash 存储。\n确定继续？"),
            QStringLiteral(
            "About to write configuration to device.\n\n"
            "This will update the flash memory on the card reader.\n"
            "Are you sure?")));

    if (result != QMessageBox::Yes) return;

    m_lblStatusMsg->setText(trC(
        QStringLiteral("正在写入设置..."),
        QStringLiteral("Writing settings to device...")));
    m_hid->requestFlashWrite(data);
}

void MainWindow::onResetDefaults()
{
    auto result = QMessageBox::question(
        this,
        trC(QStringLiteral("恢复默认"),
            QStringLiteral("Reset Defaults")),
        trC(QStringLiteral("将所有设置恢复为出厂默认值？\n未保存的更改将丢失。"),
            QStringLiteral("Reset all settings to factory defaults?\n"
                           "Unsaved changes will be lost.")));

    if (result != QMessageBox::Yes) return;

    FlashData defaults;
    memset(&defaults, 0, sizeof(FlashData));
    defaults.firmversion_1      = 0x01;
    defaults.firmversion_2      = 0x00;
    defaults.system_setting     = 0x00;   // 115200
    defaults.gobal_LED_setting  = 0x80;   // 50%
    defaults.sega_setting       = 0x79;
    defaults.spice_setting      = 0x66;
    defaults.namco_setting      = 0x66;

    populateSettingsUI(defaults);
    m_lblStatusMsg->setText(trC(
        QStringLiteral("已恢复出厂默认值（尚未写入设备）"),
        QStringLiteral("Settings reset to defaults (not yet written)")));
}

void MainWindow::onLedSliderChanged(int value)
{
    if (!m_connected) return;
    uint8_t v = static_cast<uint8_t>(value);
    m_hid->requestLedColor(v, v, v);
}

/* ---- 原始数据日志 ---- */

void MainWindow::onRawDataSent(const QByteArray &data)
{
    appendLogLine(QStringLiteral("[Send]"),
                  data.toHex(' ').toUpper(),
                  QStringLiteral("#FFD54F"));
}

void MainWindow::onRawDataReceived(const QByteArray &data)
{
    appendLogLine(QStringLiteral("[Recv]"),
                  data.toHex(' ').toUpper(),
                  QStringLiteral("#00E676"));
}

void MainWindow::appendLogLine(const QString &prefix, const QString &hex,
                               const QString &color)
{
    if (!m_rawLog) return;
    m_rawLog->append(QStringLiteral(
        "<span style=\"color:%1;font-weight:600\">%2</span> "
        "<span style=\"color:#CCCCCC\">%3</span>")
        .arg(color, prefix, hex));
    // 自动滚到底部
    auto *sb = m_rawLog->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

/* ====================================================================
 *  DFU 相关槽
 * ==================================================================== */

void MainWindow::onJumpToDfu()
{
    // 未连接时直接切换到 DFU 页面 (供刷死设备的用户使用)
    if (!m_connected) {
        m_stackedRight->setCurrentIndex(1);
        m_btnDfu->hide();
        m_btnConnect->setEnabled(true);
        m_lblStatusMsg->setText(trC(
            QString::fromUtf8("已进入 DFU 模式（离线）。请选择固件文件烧录。"),
            QStringLiteral("DFU mode (offline). Select firmware to flash.")));
        onDfuCheckTool();
        return;
    }

    auto result = QMessageBox::question(this,
        trC(QString::fromUtf8("跳转 DFU"),
            QStringLiteral("Jump to DFU")),
        trC(QString::fromUtf8(
            "读卡器将断开连接并进入 DFU 固件升级模式。\n确定继续？"),
            QStringLiteral(
            "The reader will disconnect and enter DFU mode.\nContinue?")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result != QMessageBox::Yes) return;

    m_inDfuMode = true;
    m_lblStatusMsg->setText(trC(
        QString::fromUtf8("正在请求跳转 DFU..."),
        QStringLiteral("Requesting DFU jump...")));
    m_hid->requestJumpToDfu();
}

void MainWindow::onDfuFileBrowse()
{
    QString path = QFileDialog::getOpenFileName(this,
        trC(QString::fromUtf8("选择固件文件"),
            QStringLiteral("Select Firmware File")),
        QString(),
        QStringLiteral("Firmware Files (*.bin *.hex *.dfu);;All Files (*)"));
    if (!path.isEmpty()) {
        m_dfuFilePath = path;
        m_dfuFileEdit->setText(path);
        m_dfuFlashBtn->setEnabled(true);
    }
}

void MainWindow::onDfuFlash()
{
    if (m_dfuFilePath.isEmpty()) return;

    m_dfuFlashBtn->setEnabled(false);
    m_dfuLog->clear();

    // Step 1: 列出 DFU 设备 (异步)
    m_dfuLog->appendPlainText(QString::fromUtf8("正在扫描 DFU 设备...\n"));

    QString dfuExe = m_dfuToolPath.isEmpty()
        ? QStringLiteral("dfu-util") : m_dfuToolPath;

    connect(m_dfuProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onDfuListFinished,
            Qt::SingleShotConnection);

    m_dfuProcess->start(dfuExe, {QStringLiteral("-l")});
}

void MainWindow::onDfuListFinished(int, QProcess::ExitStatus)
{
    QString out = QString::fromUtf8(m_dfuProcess->readAllStandardOutput());
    m_dfuLog->appendPlainText(out);

    if (!out.contains(QStringLiteral("Found DFU"))) {
        m_dfuLog->appendPlainText(
            QString::fromUtf8("[错误] 未检测到 DFU 设备\n"
            "请确认设备已进入 DFU 模式且 USB 驱动已安装\n"));
        m_dfuFlashBtn->setEnabled(true);
        return;
    }

    // Step 2: 烧录
    QString dfuExe = m_dfuToolPath.isEmpty()
        ? QStringLiteral("dfu-util") : m_dfuToolPath;
    QStringList args;
    args << QStringLiteral("-a") << QStringLiteral("0")
         << QStringLiteral("-s") << QStringLiteral("0x08000000:leave")
         << QStringLiteral("-D") << m_dfuFilePath;

    m_dfuLog->appendPlainText(
        QStringLiteral("\n$ dfu-util %1\n").arg(args.join(QLatin1Char(' '))));

    connect(m_dfuProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onDfuFlashFinished,
            Qt::SingleShotConnection);

    m_dfuProcess->start(dfuExe, args);
}

void MainWindow::onDfuFlashFinished(int exitCode, QProcess::ExitStatus)
{
    QString out = QString::fromUtf8(m_dfuProcess->readAllStandardOutput());
    QString err = QString::fromUtf8(m_dfuProcess->readAllStandardError());
    m_dfuLog->appendPlainText(out);
    if (!err.isEmpty()) m_dfuLog->appendPlainText(err);

    if (exitCode != 0) {
        m_dfuLog->appendPlainText(
            QString::fromUtf8("\n[失败] dfu-util 返回错误码 %1\n"
            "提示: 如刷了错误固件导致变砖, 请用 STM32CubeProgrammer 救砖\n")
                .arg(exitCode));
        m_dfuFlashBtn->setEnabled(true);
        return;
    }

    // Step 3: 复位
    m_dfuLog->appendPlainText(
        QString::fromUtf8("\n烧录完成, 正在复位设备...\n"));

    QString dfuExe = m_dfuToolPath.isEmpty()
        ? QStringLiteral("dfu-util") : m_dfuToolPath;

    connect(m_dfuProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onDfuResetFinished,
            Qt::SingleShotConnection);

    QTimer::singleShot(500, this, [this, dfuExe]() {
        m_dfuProcess->start(dfuExe, {QStringLiteral("-e")});
    });
}

void MainWindow::onDfuResetFinished(int, QProcess::ExitStatus)
{
    m_dfuLog->appendPlainText(
        QString::fromUtf8("[成功] 固件烧录完成！\n"
        "如设备无法识别请重新拔插 USB。"));

    QMessageBox::information(this,
        trC(QString::fromUtf8("完成"), QStringLiteral("Done")),
        trC(QString::fromUtf8(
            "固件烧录完成。\n\n"
            "请重新拔插设备 USB 线后再点击「连接」。"),
            QStringLiteral(
            "Firmware flashed.\n\n"
            "Please reconnect the USB cable, then click Connect.")));

    m_dfuFlashBtn->setEnabled(true);
}

/* ---- dfu-util 自动下载 ---- */

static const QStringList g_dfuMirrors = {
    // 优先 Gitee 镜像 (国内快)
    QStringLiteral("https://gitee.com/qinh/dfu-util-win64/raw/master/dfu-util.exe"),
    // GitHub 原始文件
    QStringLiteral("https://github.com/qinh/dfu-util-win64/raw/master/dfu-util.exe"),
    // SourceForge 官方
    QStringLiteral("https://downloads.sourceforge.net/project/dfu-util/dfu-util-0.11/dfu-util-0.11-binaries/win32-mingw32/dfu-util.exe"),
};
static int g_dfuMirrorIdx = 0;

void MainWindow::onDfuCheckTool()
{
    // 查找 dfu-util.exe: 优先看 exe 同目录, 再看 tools/
    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath() + QStringLiteral("/dfu-util.exe")
                << QCoreApplication::applicationDirPath() + QStringLiteral("/tools/dfu-util.exe");

    for (const auto &p : searchPaths) {
        if (QFile::exists(p)) {
            m_dfuToolPath = p;
            m_dfuDownloadBtn->setVisible(false);
            m_dfuLog->appendPlainText(
                QString::fromUtf8("[OK] 找到 dfu-util: %1\n").arg(p));
            return;
        }
    }

    // 没找到 → 显示下载按钮
    m_dfuDownloadBtn->setVisible(true);
    m_dfuDownloadBtn->setText(
        QString::fromUtf8("未找到 dfu-util.exe，点击自动下载"));
    m_dfuDownloadBtn->setToolTip(QString::fromUtf8(
        "将从可信任源下载 dfu-util 到程序目录"));

    // 也自动弹对话框问
    g_dfuMirrorIdx = 0;
    auto result = QMessageBox::question(this,
        trC(QString::fromUtf8("缺少工具"),
            QStringLiteral("Missing Tool")),
        trC(QString::fromUtf8(
            "未找到 dfu-util.exe，无法进行固件烧录。\n\n"
            "是否自动从可信任源下载？"),
            QStringLiteral(
            "dfu-util.exe not found.\n\n"
            "Download automatically from a trusted source?")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (result == QMessageBox::Yes) {
        tryDownloadDfuUtil();
    }
}

void MainWindow::tryDownloadDfuUtil()
{
    if (m_dlProgress) {
        m_dlProgress->close();
        delete m_dlProgress;
    }

    m_dlProgress = new QProgressDialog(
        trC(QString::fromUtf8("正在下载 dfu-util..."),
            QStringLiteral("Downloading dfu-util...")),
        trC(QStringLiteral("取消"), QStringLiteral("Cancel")),
        0, 0, this);
    m_dlProgress->setWindowModality(Qt::WindowModal);
    m_dlProgress->show();

    m_dfuLog->appendPlainText(
        QString::fromUtf8("正在下载 dfu-util...\n"));

    QUrl url(g_dfuMirrors.at(g_dfuMirrorIdx));
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_netManager->get(req);
}

void MainWindow::onDfuDownloadFinished(QNetworkReply *reply)
{
    if (m_dlProgress) {
        m_dlProgress->close();
        delete m_dlProgress;
        m_dlProgress = nullptr;
    }

    QByteArray data = reply->readAll();
    int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || status >= 400) {
        m_dfuLog->appendPlainText(
            QString::fromUtf8("[下载失败] %1 (HTTP %2)\n")
                .arg(reply->errorString()).arg(status));

        // 尝试下一个镜像
        g_dfuMirrorIdx++;
        if (g_dfuMirrorIdx < g_dfuMirrors.size()) {
            m_dfuLog->appendPlainText(
                QString::fromUtf8("尝试备用源: %1\n")
                    .arg(g_dfuMirrors.at(g_dfuMirrorIdx)));
            tryDownloadDfuUtil();
        } else {
            m_dfuLog->appendPlainText(
                QString::fromUtf8(
                    "[错误] 所有下载源均已失败。\n"
                    "请手动下载 dfu-util.exe 并放到程序目录下。\n"));
        }
        reply->deleteLater();
        return;
    }

    // 验证不是 HTML 错误页
    if (data.size() < 1024) {
        m_dfuLog->appendPlainText(
            QString::fromUtf8("[错误] 下载的文件过小 (%1 字节)，可能不是有效的 exe。\n")
                .arg(data.size()));
        reply->deleteLater();
        return;
    }

    // 保存到 exe 同目录
    QString savePath = QCoreApplication::applicationDirPath()
                       + QStringLiteral("/dfu-util.exe");
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_dfuLog->appendPlainText(
            QStringLiteral("[错误] 无法写入 %1\n").arg(savePath));
        reply->deleteLater();
        return;
    }
    file.write(data);
    file.close();

    m_dfuLog->appendPlainText(
        QString::fromUtf8("[OK] dfu-util 已下载到 %1 (%2 KB)\n")
            .arg(savePath).arg(data.size() / 1024));

    m_dfuDownloadBtn->setVisible(false);
    m_dfuToolPath = savePath;
    reply->deleteLater();
}

void MainWindow::onToggleLanguage()
{
    m_langEn = !m_langEn;
    applyLanguage();
}

/* ---- 更新所有 UI 文本 (中/英切换) ---- */
void MainWindow::applyLanguage()
{
    // 按钮
    m_btnConnect->setText(trC(QStringLiteral("连接"),
                              QStringLiteral("Connect")));
    m_btnDisconnect->setText(trC(QStringLiteral("断开"),
                                QStringLiteral("Disconnect")));
    m_btnLang->setText(m_langEn ? QStringLiteral("中") : QStringLiteral("EN"));
    m_btnLang->setToolTip(m_langEn
        ? QStringLiteral("切换到中文") : QStringLiteral("Switch to English"));

    m_btnRead->setText(trC(QStringLiteral("从设备读取"),
                           QStringLiteral("Read from Device")));
    m_btnWrite->setText(trC(QStringLiteral("写入设备"),
                            QStringLiteral("Write to Device")));
    m_btnReset->setText(trC(QStringLiteral("恢复默认"),
                            QStringLiteral("Reset Defaults")));

    // 状态
    updateConnectionUI(m_connected);

    // SEGA tab
    m_sega_tn32->setText(trC(
        QString::fromUtf8("启用 TN32 模拟。默认：模拟 15693 读卡器"),
        QStringLiteral("Enable TN32 emulation (default: emulate 15693 reader)")));
    m_sega_aic->setText(trC(
        QString::fromUtf8("FeliCa AIC 卡号转换（转为真实序列号）"),
        QStringLiteral("FeliCa AIC transform to real serial")));
    m_sega_nesica->setText(trC(
        QString::fromUtf8("启用 NESiCA 支持（MIFARE Ultralight）"),
        QStringLiteral("Enable NESiCA (MIFARE Ultralight)")));
    m_sega_ea_pass->setText(trC(
        QString::fromUtf8("启用 e-Amusement Pass 支持（ISO 15693）"),
        QStringLiteral("Enable e-Amusement Pass (ISO 15693)")));
    m_sega_tunion->setText(trC(
        QString::fromUtf8("启用交通联合支持（ISO 14443-A，APDU）"),
        QStringLiteral("Enable T-Union (ISO 14443-A, APDU)")));
    m_sega_jubeat->setText(trC(
        QString::fromUtf8("启用乐动魔方支持（Jubeat China，MIFARE）"),
        QStringLiteral("Enable Jubeat China (MIFARE)")));

    // SPICE tab
    m_spice_2p->setText(trC(
        QString::fromUtf8("启用 2P 模式"),
        QStringLiteral("Enable 2P mode")));
    m_spice_aime->setText(trC(
        QString::fromUtf8("启用 Aime 支持（MIFARE Classic）"),
        QStringLiteral("Enable Aime (MIFARE Classic)")));
    m_spice_banapass->setText(trC(
        QString::fromUtf8("启用 Banapass 支持（MIFARE Classic）"),
        QStringLiteral("Enable Banapass (MIFARE Classic)")));
    m_spice_nesica->setText(trC(
        QString::fromUtf8("启用 NESiCA 支持（MIFARE Ultralight）"),
        QStringLiteral("Enable NESiCA (MIFARE Ultralight)")));
    m_spice_tunion->setText(trC(
        QString::fromUtf8("启用交通联合支持（ISO 14443-A，APDU）"),
        QStringLiteral("Enable T-Union (ISO 14443-A, APDU)")));
    m_spice_jubeat->setText(trC(
        QString::fromUtf8("启用乐动魔方支持（Jubeat China，MIFARE）"),
        QStringLiteral("Enable Jubeat China (MIFARE)")));

    // NAMCO tab
    m_namco_todo->setText(trC(
        QStringLiteral("（保留）"), QStringLiteral("(Reserved)")));
    m_namco_aime->setText(trC(
        QString::fromUtf8("启用 Aime 支持（MIFARE Classic）"),
        QStringLiteral("Enable Aime (MIFARE Classic)")));
    m_namco_banapass->setText(trC(
        QString::fromUtf8("启用 Banapass 支持（MIFARE Classic）"),
        QStringLiteral("Enable Banapass (MIFARE Classic)")));
    m_namco_nesica->setText(trC(
        QString::fromUtf8("启用 NESiCA 支持（MIFARE Ultralight）"),
        QStringLiteral("Enable NESiCA (MIFARE Ultralight)")));
    m_namco_tunion->setText(trC(
        QString::fromUtf8("启用交通联合支持（ISO 14443-B）"),
        QStringLiteral("Enable T-Union (ISO 14443-B)")));
    m_namco_jubeat->setText(trC(
        QString::fromUtf8("启用乐动魔方支持（Jubeat China，MIFARE）"),
        QStringLiteral("Enable Jubeat China (MIFARE)")));

    // System tab group titles
    if (m_cmbUartBaud) {
        auto *p = m_cmbUartBaud->parentWidget();
        while (p) {
            auto *gb = qobject_cast<QGroupBox*>(p);
            if (gb) {
                QString title = gb->title();
                if (title.contains(QString::fromUtf8("波特率")) ||
                    title.contains("Baud") || title.contains("UART"))
                    gb->setTitle(trC(QString::fromUtf8("串口波特率"),
                                    QStringLiteral("UART Baudrate")));
                else if (title.contains("LED") || title.contains(QString::fromUtf8("亮度")))
                    gb->setTitle(trC(QString::fromUtf8("LED 全局亮度"),
                                    QStringLiteral("Global LED Brightness")));
                break;
            }
            p = p->parentWidget();
        }
    }

    // 卡片区域
    if (!m_connected || !m_lblCardUid->property("hasCard").toBool())
        clearCardDisplay();

    // 状态栏
    m_lblStatusMsg->setText(trC(
        QStringLiteral("就绪 — 点击连接开始"),
        QStringLiteral("Ready — Click Connect to start")));
}

// ====================================================================
//  HIDWorker 信号响应
// ====================================================================

void MainWindow::onDeviceConnected(const QString &desc)
{
    m_connected = true;
    updateConnectionUI(true);
    setControlsEnabled(true);
    m_btnConnect->setEnabled(true);
    m_lblStatusMsg->setText(trC(
        QStringLiteral("已连接: %1  |  正在读取设置...").arg(desc),
        QStringLiteral("Connected: %1  |  Reading settings...").arg(desc)));
    m_hid->requestFlashRead();
}

void MainWindow::onDeviceDisconnected()
{
    m_connected = false;
    m_btnConnect->setEnabled(true);
    updateConnectionUI(false);
    clearCardDisplay();
    m_lblFwVersion->setText(QStringLiteral("FW: --"));
    m_lblReaderModel->hide();

    if (m_inDfuMode) {
        m_inDfuMode = false;
        setControlsEnabled(false);
        m_stackedRight->setCurrentIndex(1);
        m_btnDfu->hide();
        m_lblStatusMsg->setText(trC(
            QString::fromUtf8("设备已进入 DFU 模式，请选择固件文件烧录。"),
            QStringLiteral("Device entered DFU mode. Select firmware to flash.")));
        onDfuCheckTool();   // 检查 dfu-util 是否可用
    } else {
        setControlsEnabled(false);
        m_lblStatusMsg->setText(trC(
            QStringLiteral("设备已断开"),
            QStringLiteral("Device disconnected")));
    }
}

void MainWindow::onDeviceError(const QString &msg)
{
    m_btnConnect->setEnabled(true);
    QMessageBox::warning(this,
        trC(QStringLiteral("设备错误"), QStringLiteral("Device Error")), msg);
    m_lblStatusMsg->setText(trC(
        QStringLiteral("错误: ") + msg,
        QStringLiteral("Error: ") + msg));
}

void MainWindow::onCardDetected(uint8_t cardType, const QByteArray &rawData)
{
    QString uid        = extractUid(cardType, rawData);
    QString typeName   = cardTypeName(cardType);
    QString cardNumber = extractCardNumber(cardType, rawData);

    showCardInDisplay(uid, typeName, cardNumber);
    m_lblStatusMsg->setText(
        QStringLiteral("Card: %1  |  %2  |  %3 bytes")
            .arg(uid, typeName).arg(rawData.size()));
}

void MainWindow::onFlashDataRead(const FlashData &data,
                                 uint8_t readerType, uint8_t fwVersion)
{
    m_flashData = data;
    populateSettingsUI(data);

    int fwMajor = fwVersion >> 4;
    int fwMinor = fwVersion & 0x0F;
    m_lblFwVersion->setText(
        QStringLiteral("FW: v%1.%2").arg(fwMajor).arg(fwMinor));

    QString modelStr;
    switch (readerType) {
        case Reader_OpenSource: modelStr = QStringLiteral("Monica OpenSource"); break;
        case Reader_Lite:       modelStr = QStringLiteral("Monica Lite");       break;
        case Reader_Standard:   modelStr = QStringLiteral("Monica Standard");   break;
        case Reader_Pro:        modelStr = QStringLiteral("Monica Pro");        break;
        default:                modelStr = QStringLiteral("Monica (0x%1)")
                                              .arg(readerType, 2, 16, QLatin1Char('0'));
    }
    m_lblReaderModel->setText(modelStr);
    m_lblReaderModel->show();

    m_lblStatusMsg->setText(
        QStringLiteral("Settings loaded  |  %1, FW v%2.%3")
            .arg(modelStr).arg(fwMajor).arg(fwMinor));
}

void MainWindow::onFlashDataWritten(bool ok)
{
    if (ok) {
        m_flashData = collectSettingsFromUI();
        m_lblStatusMsg->setText(trC(
            QStringLiteral("设置已成功写入设备"),
            QStringLiteral("Settings written to device successfully")));
        QMessageBox::information(this,
            trC(QStringLiteral("成功"), QStringLiteral("Success")),
            trC(QStringLiteral("配置已写入设备。"),
                QStringLiteral("Configuration has been written to the device.")));
    } else {
        m_lblStatusMsg->setText(trC(
            QStringLiteral("写入失败"),
            QStringLiteral("Failed to write settings")));
        QMessageBox::critical(this,
            trC(QStringLiteral("错误"), QStringLiteral("Error")),
            trC(QStringLiteral("写入设备失败。"),
                QStringLiteral("Failed to write configuration to device.")));
    }
}

// ====================================================================
//  样式表 (与之前一致, 加 lang 按钮)
// ====================================================================

void MainWindow::applyStyleSheet()
{
    const QString qss = QStringLiteral(R"(
        * {
            font-family: "Segoe UI", "Microsoft YaHei", "PingFang SC", sans-serif;
            font-size: 13px;
        }
        QMainWindow { background-color: #F3F3F3; }

        QFrame#headerPanel {
            background-color: #FFFFFF;
            border-bottom: 1px solid #E0E0E0;
        }
        QLabel#appTitle {
            font-size: 18px; font-weight: 600; color: #1A1A1A; padding: 0 8px;
        }
        QLabel#connDot {
            border-radius: 5px; min-width: 10px; min-height: 10px;
            max-width: 10px; max-height: 10px;
        }
        QLabel#connDot[status="connected"]    { background-color: #16C60C; }
        QLabel#connDot[status="disconnected"] { background-color: #CCCCCC; }
        QLabel#connStatus { font-size: 12px; color: #616161; padding-left: 6px; }
        QLabel#fwVersion   { font-size: 12px; color: #0078D4; font-weight: 500; }
        QLabel#readerModel { font-size: 12px; color: #616161; padding-right: 8px; }

        QPushButton#btnLang {
            background-color: #F3F3F3; color: #616161;
            border: 1px solid #D0D0D0; border-radius: 4px;
            font-size: 12px; font-weight: 600; padding: 2px 0;
        }
        QPushButton#btnLang:hover { background-color: #E8E8E8; }

        QPushButton#btnDfu {
            background-color: #FF8C00; color: #FFFFFF; border: none;
            border-radius: 4px; padding: 6px 16px; font-weight: 600; min-height: 30px;
        }
        QPushButton#btnDfu:hover { background-color: #E67E00; }

        QPushButton#btnConnect {
            background-color: #0078D4; color: #FFFFFF; border: none;
            border-radius: 4px; padding: 6px 20px; font-weight: 500; min-height: 30px;
        }
        QPushButton#btnConnect:hover  { background-color: #106EBE; }
        QPushButton#btnConnect:pressed { background-color: #005A9E; }
        QPushButton#btnConnect:disabled { background-color: #CCCCCC; color: #999; }

        QPushButton#btnDisconnect {
            background-color: #D13438; color: #FFFFFF; border: none;
            border-radius: 4px; padding: 6px 20px; font-weight: 500; min-height: 30px;
        }
        QPushButton#btnDisconnect:hover { background-color: #C42B2F; }

        QWidget#cardMonitorPanel { background-color: #FAFAFA; }
        QFrame#cardDisplay {
            background-color: #1E1E2E; border-radius: 12px; border: 1px solid #2D2D44;
        }
        QLabel#cardUidTitle {
            color: #7C7C9A; font-size: 11px; font-weight: 600; letter-spacing: 2px;
        }
        QLabel#cardUidValue {
            color: #9090A0; font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 13px; font-weight: 400; letter-spacing: 1px; padding: 2px 0;
        }
        QLabel#cardUidValue[hasCard="true"] { color: #9090A0; }
        QLabel#cardNumberLabel {
            color: #FFD54F; font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 18px; font-weight: 700; letter-spacing: 1px; padding: 4px 0;
        }
        QLabel#cardNumberLabel[placeholder="true"] {
            color: #4A4A6A; font-weight: 400;
        }
        QLabel#cardTypeLabel { color: #82B1FF; font-size: 12px; font-weight: 600; }

        QLabel#sectionTitle {
            font-size: 11px; font-weight: 700; color: #8A8A8A; letter-spacing: 2px; padding: 4px 0;
        }

        /* === Raw Data Log === */
        QTextEdit#rawLog {
            background-color: #1A1A2E;
            color: #CCCCCC;
            font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 12px;
            border: 1px solid #2D2D44;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: #333355;
        }

        QPushButton#btnSecondary {
            background-color: #FFFFFF; color: #616161; border: 1px solid #D0D0D0;
            border-radius: 4px; padding: 4px 16px; font-size: 12px;
        }
        QPushButton#btnSecondary:hover { background-color: #F3F3F3; border-color: #B0B0B0; }

        QWidget#settingsPanel { background-color: #FFFFFF; }
        QTabWidget#settingsTabs::pane {
            border: 1px solid #E0E0E0; border-radius: 8px; background-color: #FFFFFF;
        }
        QTabBar::tab {
            background: transparent; color: #616161; padding: 8px 22px;
            border: none; border-bottom: 2px solid transparent; margin-right: 2px;
            font-size: 13px; font-weight: 500;
        }
        QTabBar::tab:selected { color: #0078D4; border-bottom: 2px solid #0078D4; }
        QTabBar::tab:hover:!selected {
            color: #323130; background-color: rgba(0,0,0,0.03); border-radius: 4px 4px 0 0;
        }

        QLabel#tabDesc { color: #8A8A8A; font-size: 12px; line-height: 1.6; }
        QCheckBox#settingCheck { spacing: 10px; color: #323130; font-size: 13px; padding: 3px 0; }
        QCheckBox#settingCheck::indicator {
            width: 18px; height: 18px; border: 2px solid #A0A0A0;
            border-radius: 3px; background-color: #FFFFFF;
        }
        QCheckBox#settingCheck::indicator:checked {
            background-color: #0078D4; border-color: #0078D4;
        }
        QCheckBox#settingCheck::indicator:hover { border-color: #0078D4; }

        QGroupBox#sysGroup {
            background-color: #FAFAFA; border: 1px solid #E8E8E8;
            border-radius: 6px; margin-top: 8px; padding: 16px 20px;
            font-weight: 600; color: #323130;
        }
        QGroupBox#sysGroup::title { subcontrol-origin: margin; left: 16px; padding: 0 6px; }

        QComboBox#sysCombo {
            border: 1px solid #C0C0C0; border-radius: 4px; padding: 6px 12px;
            background-color: #FFFFFF; color: #323130; min-height: 30px;
        }
        QComboBox#sysCombo:hover { border-color: #0078D4; }
        QComboBox#sysCombo::drop-down { border: none; width: 28px; }

        QSlider#ledSlider::groove:horizontal {
            height: 4px; background: #E0E0E0; border-radius: 2px;
        }
        QSlider#ledSlider::handle:horizontal {
            width: 16px; height: 16px; background: #0078D4;
            border-radius: 8px; margin: -6px 0;
        }
        QSlider#ledSlider::handle:horizontal:hover { background: #106EBE; }

        QLabel#ledValue {
            color: #323130; font-size: 13px; font-weight: 600;
        }
        QLabel#sysHint { color: #A0A0A0; font-size: 11px; padding-left: 4px; }

        QPushButton#btnPrimary {
            background-color: #0078D4; color: #FFFFFF; border: none;
            border-radius: 4px; padding: 8px 24px; font-weight: 500; min-height: 34px;
        }
        QPushButton#btnPrimary:hover  { background-color: #106EBE; }
        QPushButton#btnPrimary:pressed { background-color: #005A9E; }
        QPushButton#btnPrimary:disabled { background-color: #CCCCCC; color: #999999; }

        QStatusBar#appStatusBar {
            background-color: #FFFFFF; border-top: 1px solid #E0E0E0; padding: 4px 12px;
        }
        QLabel#statusMsg { color: #616161; font-size: 12px; }
    )");

    qApp->setStyleSheet(qss);
}

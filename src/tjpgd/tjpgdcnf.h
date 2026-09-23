/*----------------------------------------------*/
/* TJpgDec System Configurations R0.03          */
/*                                              */
/* CONFIGURATION ONLY - the algorithm in        */
/* tjpgd.c is ChaN's, unmodified. This file is  */
/* the one the author intends you to edit.      */
/*   JD_FORMAT     0 -> 1   RGB565 out, which   */
/*                 is what every panel here     */
/*                 wants; RGB888 would mean an  */
/*                 extra conversion per pixel.  */
/*   JD_FASTDECODE 0 -> 1   the 32-bit path.    */
/*                 ESP32-S3 is a 32-bit MCU;    */
/*                 0 is the 8/16-bit setting.   */
/*----------------------------------------------*/
#define	JD_SZBUF		512
/* Specifies size of stream input buffer */

#define JD_FORMAT		1
/* Specifies output pixel format.
/  0: RGB888 (24-bit/pix)
/  1: RGB565 (16-bit/pix)
/  2: Grayscale (8-bit/pix)
*/

#define	JD_USE_SCALE	1
/* Switches output descaling feature.
/  0: Disable
/  1: Enable
*/

#define JD_TBLCLIP		1
/* Use table conversion for saturation arithmetic. A bit faster, but increases 1 KB of code size.
/  0: Disable
/  1: Enable
*/

#define JD_FASTDECODE	1
/* Optimization level
/  0: Basic optimization. Suitable for 8/16-bit MCUs.
/  1: + 32-bit barrel shifter. Suitable for 32-bit MCUs.
/  2: + Table conversion for huffman decoding (wants 6 << HUFF_BIT bytes of RAM)
*/


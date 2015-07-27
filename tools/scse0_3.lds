OUTPUT_ARCH(mips)
/*
指定输出可执行文件的平台为mips
*/
ENTRY(_start)
/*
指定输出可执行文件的起始代码段为_start
*/
SECTIONS
{

	. = 0x10000; /*load code(start kenerl) at such address*/

	. = 0x80000080;	/*load exception handler at such address*/
	
	.except_vec3 : {
		     *(.text.exc_vec3)
		       }

	. = 0x80010000;	/*load data at such address*/

	.text : {
		*(.text)
		}

	.bss : {
             *(.dynbss)
             *(.bss)
             }
	
	.data : {
	      *(.data) 
	      }

	.sdata : {
	      *(.sdata)
	      }
	

/*To do:
  fill in the correct address of the key section
  such as text data bss ...
*/
	. = 0x80400000;

	end = .;
}

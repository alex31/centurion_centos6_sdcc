# Calling CENTOS JCL from C

Include `<os6.h>` and link with `run/cpu6-os6-sdcc`. `system_jcl()` passes a
text job stream to the native interpreter in a separate CENTOS partition.
The calling C program resumes with the captured `SYSLOG` output.

```c
char listing[512];
uint16_t size = sizeof(listing);
int status = system_jcl(".JOB LISTING ABORT=CANCEL\n.DIR 1 (S)\n.END\n",
                        listing, &size);
```

The input value of `*size` is the capacity including room for NUL. On return
it counts the copied bytes, excluding NUL. A positive capacity always leaves
the result NUL-terminated. A zero capacity permits a null output pointer.

| Return value | Meaning |
| --- | --- |
| `OS6_JCL_OK` | Complete `SYSLOG` output copied |
| `OS6_JCL_TRUNCATED` | `SYSLOG` output exceeded the caller's capacity |
| `OS6_JCL_ARGUMENT` | Invalid argument or job text |
| `OS6_JCL_IO` | Spool or transport I/O failed |
| `OS6_JCL_TIMEOUT` | The native dispatcher did not complete within the wait interval |

The job text uses ASCII records separated by LF, CRLF or CR. The result
uses LF between native log records. The return value describes submission
and capture. The job's diagnostics and native messages are part of the log.

## Native setup

The application image already contains the spool library and the test
executable. On a separate CENTOS image, reserve the `JCLSDK` library on disk 1
and run these commands once to provision its input and output files.

```text
.NEW JCLSDK ON 1 'L' 256S
.NEW JCLSDK.REQ ON 1 'A' 48S
.NEW JCLSDK.LOG ON 1 'A' 128S
.NEW JCLSDK.TAG0 ON 1 'A' 1S
```

Use an available partition 1, with `CRT1` reserved as the job console.
Connect the transmit side of `CRT2` to the receive side of `CRT1` using a
one-way serial link with compatible terminal settings and flow control.
Before running the calling program, start the job console and assign the
transmit device.

```text
.START P1 WITH CRT1
.USE CRT2 FOR SYS001
```

Use one caller at a time for this spool and console. The library writes a
native type A request and submits it by name. The separate interpreter routes
`SYSLOG` to `JCLSDK.LOG`. The CR/LF on submission invokes the request and
accepts the default answer to the native console-output prompt on CENTOS 7.13.
The generated ending closes the native log, switches
the completion tag between `TAG0` and `TAG1`, and restores the job console for
the next call. The C caller polls that tag before opening the log. This
keeps the reader out of the log while CENTOS is assigning it to the job.
Use batch streams that reach the generated ending and keep the assigned
streams available. Explicit device output goes to its chosen device. An
operator prompt in the job is handled at the job console. A timeout reports
failure to complete capture and the job console retains its native state.
The wait also has a polling limit for systems with a stopped CENTOS clock.

## Run the example from the application image

Start the emulator with its virtual serial lead from `CRT2` to `CRT1`.

```sh
CENTURION_JCL_LOOPBACK=1 run/centurion-system
```

After booting CENTOS, enter these lines at the `CRT0` console.

```text
.START P1 WITH CRT1
.USE CRT2 FOR SYS001
.RUN SDCC.XJCLTEST
```

When `CRT1` shows `JCL TEST WAITING`, press Enter on `CRT1`. The C program
prints the complete, truncated and zero-capacity test results on `CRT0`.

## Automated test

The standalone test creates a temporary boot disk. Use `--installed` to
exercise the copy of `XJCLTEST` already in the application image. Both modes
emulate the one-way lead and cover a paused multi-sector job, `.DIR` output,
truncation and a zero-size buffer.

```sh
make sdk
node tests/system-jcl.js /path/to/CENTOS_13.IMG
node tests/system-jcl.js --installed build/out/image/centos-apps.img
```

The native commands and logical units are described in the
[Centurion JCL reference](https://github.com/Nakazoto/CenturionComputer/wiki/JCL-(Job-Control-Language)).

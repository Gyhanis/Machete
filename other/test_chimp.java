package gr.aueb.delorean.chimp;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

// args[0]: data directory
// args[1]: buffer size
// args[2]: perf_file prefix
public class MyMain {
        public static void main(String[] args) throws IOException{
                File[] files = new File(args[0]).listFiles();
                int bufsize = Integer.parseInt(args[2]);
                byte[] buffer = new byte[bufsize * 8];
                double[] data = new double[bufsize];
                double[] data2 = new double[bufsize];
                long encodeTime = 0;
                long decodeTime = 0;
                long encodeSize = 0;
                long originSize = 0;
                for (File file : files) {
                        System.out.printf("Current file:%s\r", file.getName());
                        InputStream stream = new FileInputStream(file);
                        while (true) {
                                int cnt = stream.read(buffer)/8;
                                originSize += cnt * 8;
                                if (cnt == 0) break;
                                for (int i = 0; i < cnt; i++) {
                                        long datal = buffer[i*8+7] & 0xff;
                                        datal <<= 8; datal |= buffer[i*8+6] & 0xff;
                                        datal <<= 8; datal |= buffer[i*8+5] & 0xff;
                                        datal <<= 8; datal |= buffer[i*8+4] & 0xff;
                                        datal <<= 8; datal |= buffer[i*8+3] & 0xff;
                                        datal <<= 8; datal |= buffer[i*8+2] & 0xff;
                                        datal <<= 8; datal |= buffer[i*8+1] & 0xff;
                                        datal <<= 8; datal |= buffer[i*8] & 0xff;
                                        data[i] = Double.longBitsToDouble(datal);
                                }
                                ChimpN encoder = new ChimpN(128);
                                long start = System.nanoTime();
                                for (double d: data) {
                                        encoder.addValue(d);
                                }
                                encoder.close();
                                encodeTime += System.nanoTime() - start;
                                encodeSize += encoder.getSize()/8;

                                ChimpNDecompressor decoder = new ChimpNDecompressor(encoder.getOut(), 128);
                                start = System.nanoTime();
                                for (int i = 0; i < cnt; i++) {
                                        data2[i] = decoder.readValue();
                                }
                                decodeTime += System.nanoTime() - start;
                                for (int i = 0; i < cnt; i++) {
                                        if (data[i] != data2[i]) {
                                                System.out.println("Compression error!\n");
                                                System.exit(-1);
                                        }
                                }
                        }
                        stream.close();
                }
                System.out.println("Test finished                             \n");
                System.out.printf("Chimp128 compression ratio: %f\n", (double)originSize/encodeSize, originSize, encodeSize);
                double ets = (double)encodeTime / 1000 / 1000 / 1000;
                double dts = (double)decodeTime / 1000 / 1000 / 1000;
                System.out.printf("Chimp128 compression speed: %fMiB/s\n", (double)originSize/1024/1024/ets, ets);
                System.out.printf("Chimp128 decompression speed: %fMiB/s\n", (double)originSize/1024/1024/dts, dts);
        }
}

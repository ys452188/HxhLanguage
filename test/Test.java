import java.util.Objects;

public class Test {

    static class C {
        public float c;
        public D d; 
    }

    static class B extends C {
        public int b;
        public C c; 
    }

    static class A {
        public B a;
    }

    static class D {
        public String a;
    }

    // 模拟 __hx_write_string__
    public static void __hx_write_string__(String str) {
        System.out.print(str);
    }

    // fun:f(num:int, num1:int)-> int
    public static int f(int num, int num1) {
        __hx_write_string__("call f()\n");
        // 1919810 + num × num1 ÷ 2
        return 1919810 + num * num1 / 2;
    }

    // fun:g()-> void
    public static void g() {
        __hx_write_string__("Hello, HxHLanguage!\n");
        g();
    }

    public static void main(String[] args) {
        long startTime = System.nanoTime();
        int 新变量;
        新变量 = 114;
        
        int newVar1 = 0;
        if (新变量 == 114 || false) { 
            newVar1 = 114;
        }
        
        newVar1 = f(newVar1, 新变量);
        __hx_write_string__("Hello, HxHLanguage!\n");
        
        float newFloat;
        newFloat = (float)(114.514 + newVar1);
        
        f((int)newFloat, 114);
        
        新变量 = 0;
        // repeat -> ... until：直到满足条件才停止，即条件满足就跳出。
        // 等价于 Java 的 do-while(!条件)
        do {
            __hx_write_string__("Hello, HxHLanguage!\n");
            新变量 = 新变量 + 114;
        } while (!(新变量 > newFloat)); // 或者是 新变量 <= newFloat
        
        int 返回值 = 0; // 对应“返回：0”
        // ----------------------------------
        long endTime = System.nanoTime();
        
        long durationNano = endTime - startTime;
        double durationMilli = durationNano / 1_000_000.0;

        System.out.printf("总耗时: %f s\n", durationMilli/1000.0);
    }
}
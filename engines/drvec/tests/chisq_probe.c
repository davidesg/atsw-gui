/*  chisq_probe — la chi2 acumulada del motor, contra GSL.
 *
 *  POR QUE EXISTE.  chisq() de nlatools.c tenia BUG-13: con df >= 30 aplicaba
 *  DOS correcciones de cola, de modo que para z < 0 -- un estadistico por
 *  debajo de su media, o sea el caso en que el modelo esta BIEN -- devolvia el
 *  complemento de la CDF.  Todo p-valor escrito como `1.0 - chisq(...)` salia
 *  invertido y la diagnosis declaraba no blancos unos residuos limpios.
 *
 *  drvec no llama a chisq: usa gsl_cdf_chisq_Q y lo dice en diagnose_mv.c.  Eso
 *  hace que su bateria NO proteja el arreglo, que es justo lo que este arnes
 *  viene a resolver -- la funcion es del fichero COMPARTIDO, y si aqui se
 *  rompe se rompe en drvarma y en drtran, que si la usan.
 *
 *  Imprime una linea por caso: x df chisq gsl |diferencia|.                   */
#include <stdio.h>
#include <math.h>
#include <gsl/gsl_cdf.h>
#include "main.h"

real macheps = 2.220446049250313e-16;   /* lo pide choldcp al enlazar */

int main(void)
{
    /*  Los tres primeros son z < 0 con df >= 30, que es donde vivia el
     *  defecto; los siguientes cubren z > 0 y la rama exacta de df < 30.     */
    static const double cs[][2] = {
        {23.4777, 40}, {30.0, 40}, {110.0, 126}, {29.0, 30},
        {45.0, 40}, {60.0, 40}, {200.0, 150}, {15.0, 20}, {5.0, 10}
    };
    int i;
    for (i = 0; i < (int) (sizeof cs / sizeof cs[0]); i++) {
        double x = cs[i][0]; int df = (int) cs[i][1];
        double a = chisq(x, df), b = gsl_cdf_chisq_P(x, df);
        printf("%.4f %d %.8f %.8f %.3e\n", x, df, a, b, fabs(a - b));
    }
    return 0;
}

#ifndef __CIRQ_H__
#define __CIRQ_H__

/*
 * Define function prototypes.
 */
void mt_cirq_enable(void);
void mt_cirq_disable(void);
void mt_cirq_clone_gic(void);
void mt_cirq_flush(void);
int mt_cirq_test(void);
void mt_cirq_dump_reg(void);



#endif /*!__CIRQ_H__ */

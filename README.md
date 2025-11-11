A parte de semáforo dos carros foi feita em conjunto com o Ricardo Peloso, para garantir que os semáforos estivessem sincronizados e operando conjuntamente.

Implementamos um botão de sincronização para fazer com que ambos microcontroladores operem ao mesmo tempo quando pressionado. Esse processo foi testado e validado durante as semanas da atividade. O sinal é enviado por meio do pino PTA5.

Cada led possui sua própria thread, e opera mediante uma variável que determina qual deve ser priorizada durante o ciclo. Ou seja, se está em 0, liga verde. Se está em 1, liga amarelo. E se está em 2, liga vermelho.

O modo noturno também foi implementado no código, porém, como não há nenhuma forma específica de determinar o tempo atual, o código está pronto mas não tem utilidade. Poderia ser implementado um botão que ativasse o modo noturno, mas preferimos não fazer isso por conta do tempo limitado e da dificuldade da atividade. Apesar disso, o código é funcional.




#ifndef AUD_VGM_CONFIGURE_H_
#define AUD_VGM_CONFIGURE_H_

typedef struct {
    int num_loops;
    double fade_length;

    int chip_sel;

} AudaciousVgmConfig;

extern AudaciousVgmConfig audcfg;

#endif // AUD_VGM_CONFIGURE_H_

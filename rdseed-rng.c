#include <asm/archrandom.h>
#include <linux/hw_random.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>

//The Intel DRNG Software Development Guide
//recommends to try 10 times in a tight loop.
//The probability of 10 sucessive failures are
//prohibitively small and are indicative of a
//hardware failure.
#define NUM_RDRAND_RETRIES 10

static int rdrand_rng_read(struct hwrng *rng, void *data, size_t max, bool wait){
    long seed = 0;
    int i = 0;
    const size_t to_copy = min(sizeof(seed), max);
    (void) wait;

    for (i = 0; i < NUM_RDRAND_RETRIES; i++) {
        if (likely(arch_get_random_long(&seed))) {
            memcpy(data, &seed, to_copy);
            return to_copy;
        }
    }
    printk(KERN_ERR "Could not successfully call RDRAND with %d retries.\n", NUM_RDRAND_RETRIES);
    return 0;
}

static int rdseed_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
    long seed = 0;
    const size_t to_copy = min(sizeof(seed), max);
    (void) wait;

    //if RDSEED does not produce a value, fall back to RDRAND
    if (unlikely(!arch_get_random_seed_long(&seed))) {
        return rdrand_rng_read(rng, data, max, wait);
    }
    memcpy(data, &seed, to_copy);
    return to_copy;
}

static struct hwrng intel_rng = {
	.name		= "rdseed",
	.init		= NULL,
	.cleanup	= NULL,
	.read	    = rdseed_rng_read,
    .quality    = 128 // entropy per mill; conservative guess
};

static int __init mod_init(void)
{
	int err = -ENODEV;
    if (!arch_has_random_seed()) {
        printk(KERN_INFO "No RDSEED instruction supported. Trying to fall back on RDRAND\n");
        intel_rng.read = rdrand_rng_read;
        //TODO: adjust entropy level
    }
    if (!arch_has_random()) {
        printk(KERN_ERR "No RDRAND instruction support. Giving up...\n");
        goto out;
    }

	err = hwrng_register(&intel_rng);
	if (err) {
		printk(KERN_ERR "Could not register rdseed-rng (%d)\n",
		       err);
	}
out:
    return err;
}

static void __exit mod_exit(void)
{
	hwrng_unregister(&intel_rng);
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_DESCRIPTION("H/W RNG driver for Intel's RDRAND/RDSEED instructions");
MODULE_LICENSE("GPL");

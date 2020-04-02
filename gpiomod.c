#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

/* 
define gpios for outputs
*/
static struct gpio leds[] = {
             {4, GPIOF_OUT_INIT_LOW, "LED 1"},

};

/*define gpios for input*/
static struct gpio buttons[] = {
              {98, GPIOF_IN, "BUTTON 1"},
             // {92, GPIOF_IN, "BUTTON 2"},

};

/*Later, the assigned IRQ numbers for the buttons are stored here*/
static int button_irqs[]={-1,-1};

/*the interrupt service routine called on button presses*/
static irqreturn_t button_isr(int irq, void *data)
{
    if(irq==button_irqs[0] && !gpio_get_value(leds[0].gpio)){

                  gpio_set_value(leds[0].gpio, 1);
    }
   // else if(irq == button_irqs[1] && gpio_get_value(leds[0].gpio)){
   //               gpio_set_value(leds[0].gpio, 0);


   printk(KERN_ERR "entering interrupt handling\n");




   // }
    return IRQ_HANDLED;



}



static int __init gpiomod_init(void){

  int ret = 0;

  printk(KERN_INFO "%s\n", __func__ );
  // stuff to do
//regiser led gpios
  ret = gpio_request_array(leds, ARRAY_SIZE(leds));

  if(ret){

      printk(KERN_ERR "Uable to request GPIOs for LEDs: %d\n",ret);
      return ret;

  }

  //register Button gpios
  ret = gpio_request_array(buttons, ARRAY_SIZE(buttons));

  if (ret) {
        printk(KERN_ERR "I am Unable to request GPIOs for Buttons: %d\n", gpio_get_value(buttons[0].gpio));

        goto fail1;

  }



  printk(KERN_INFO "Current button1 value:%d\n", gpio_get_value(buttons[0].gpio));
 // printk(KERN_INFO "Current button1 pin:%d\n", buttons[0].gpio);
  ret = gpio_direction_input(buttons[0].gpio);

  if(ret < 0) {
            printk(KERN_ERR " Unable to set gpio direction input: %d\n", ret);
                    goto fail2;
                              }
  printk(KERN_ERR " Pass Gpio direction setting: %d\n", ret);

  ret = gpio_to_irq(buttons[0].gpio); //get the IRQ number of the specific GPIO


  if(ret < 0) {
        printk(KERN_ERR "1 Unable to request IRQ: %d\n", ret);
        goto fail2;
          }

  button_irqs[0] = ret;

  printk(KERN_INFO "Successfully requested BUTTON1 IRQ: %d\n", button_irqs[0]);

  ret = request_irq(button_irqs[0], button_isr, IRQF_TRIGGER_RISING /* | IRQF_DISABLED */, "gpiomod#button1", NULL);

if(ret) {
           printk(KERN_ERR "2 Unable to request IRQ: %d\n", ret);
           goto fail2;
            }
/*
  ret = gpio_to_irq(buttons[1].gpio);

  if(ret < 0) {
          printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
          goto fail2;
            }
  
  button_irqs[1] = ret;

  printk(KERN_INFO "Successfully requested BUTTON2 IRQ: %d\n", button_irqs[1]);

  */

//  ret = request_irq(button_irqs[1], button_isr, IRQF_TRIGGER_RISING /* | IRQF_DISABLED */, "gpiomod#button2", NULL);

  // if(ret) {
    //          printk(KERN_ERR  "3 Unable to request IRQ: %d\n", ret);
      //        goto fail3;
        //        }

  return 0;
// cleanup what has been setup so far

  fail2:
         gpio_free_array(buttons, ARRAY_SIZE(leds));

  fail1:
         gpio_free_array(leds, ARRAY_SIZE(leds));

  return ret;




}

static void __exit gpiomod_exit(void){
  int i;

  printk(KERN_INFO "%s\n",__func__);
  //free irqs
  // stuff to do

  free_irq(button_irqs[0],NULL);
free_irq(button_irqs[1],NULL);

  //turn off all LEDs

  for (i=0; i<ARRAY_SIZE(leds); i++){

  gpio_set_value(leds[i].gpio,0);

  }

  // unregister
  gpio_free_array(leds, ARRAY_SIZE(leds));
  gpio_free_array(buttons, ARRAY_SIZE(buttons));



  printk(KERN_INFO "gpio_kernel: stopping done.");
}

module_init(gpiomod_init);
module_exit(gpiomod_exit);

MODULE_LICENSE("GPL");
MODULE_INFO(vermagic, "4.14.29-libre-computer-02957-g0567c68c5f4d-dirty SMP mod_unload aarch64");


